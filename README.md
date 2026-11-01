# Pub-Sub over UDP (C++) + Wireshark

A small streaming system built on **UDP datagrams** in C++. A publisher sends
video frames to a broker, and the broker passes them on to every subscriber of
that stream. Wireshark can show every packet on the wire.

```
Publisher ──packets──▶  BROKER  ──packets──▶ Subscriber A
 (frames cut into                            Subscriber B
  1200-byte pieces)
```

## Files

| File | What it does |
|---|---|
| `protocol.h` | The packet layout (32-byte header + data). Read this first. |
| `broker.cpp` | Remembers who subscribed to what, forwards DATA packets to them. Can simulate packet loss. |
| `publisher.cpp` | Reads `frame001.png`, `frame002.png`... and sends each one as several packets. |
| `subscriber.cpp` | Subscribes, collects packets, rebuilds the frames into `received/`, reports lost packets. |
| `wireshark/pubsub.lua` | Wireshark plugin that decodes our packets field by field. |
| `frames/` | 20 sample PNG frames. |
| `Makefile` | `make` builds everything, `make clean` removes the build. |
| `Computer netweroking pub-sub model.pdf` | Original design notes (if present). |

The earlier Java/TCP version is in git history (commit `3a1bb11`).

## Run it

Needs a C++ compiler (macOS: `xcode-select --install`). Open 3 terminals in this folder.

```bash
make
```

| Terminal | Command | Type in |
|---|---|---|
| 1 | `./broker` | port `9090`, loss `0` (use `10` to simulate 10% loss) |
| 2 | `./subscriber` | port `9090`, stream `movie` |
| 3 | `./publisher` | port `9090`, stream `movie`, folder `frames` |

Start them in that order: the subscriber must be running before the publisher
starts, because the broker keeps no history (UDP, live only). Use port 9090,
not 5000 (macOS AirPlay uses 5000). **Output files:** the subscriber creates a
`received/` folder in the directory you run it from and saves each rebuilt frame
there as `received/frame_001.png`, `frame_002.png`, ... (open them in Finder to
see the images). `make clean` deletes this folder. The
subscriber waits as long as needed for the first packet (so you can take your time
starting the publisher), then stops by itself after 3 seconds of silence and prints how many
packets and frames were lost.

Tested on the 20 sample frames: 199 packets, 0 lost, rebuilt files identical
to the originals. With 10% loss: about 22 packets lost and only 6 of 19 frames
complete.

## The packet (application-layer protocol)

Every UDP datagram is one packet: a 32-byte header followed by up to 1200 bytes of data.
Numbers are in network byte order (big-endian): `htons`/`htonl` when sending, `ntohs`/`ntohl` when reading.
The struct uses plain `int` (4 bytes) and `short` (2 bytes), which is what they are on a Mac, and the field
order (4-byte fields first) means the compiler adds no padding, so the header is exactly 32 bytes.

| Bytes | Field | Meaning |
|---|---|---|
| 0-3 | seq | counts every packet the publisher sends; a missing number = a lost packet |
| 4-7 | frame_id | which video frame this piece belongs to |
| 8-9 | chunk_idx | which piece of the frame this is (0, 1, 2...) |
| 10-11 | chunk_total | how many pieces the frame has |
| 12-13 | len | bytes of data in this packet |
| 14-15 | type | 1 SUBSCRIBE, 2 UNSUBSCRIBE, 3 DATA |
| 16-31 | stream | stream name, e.g. `movie` |
| 32+ | data | a slice of the frame |

Why 1200 bytes of data? A network link carries at most about 1500 bytes per packet (the MTU).
1200 + 32 + 8 (UDP header) + 20 (IP header) = 1260, so the packet is never split along the way.

## How the code works (for explaining it)

The code is kept deliberately simple and written for this one Mac (localhost, plain `int`/`short`, no
input checks), so every line can be explained. Each file starts with a short summary and has a short
comment on each line or block: what it does, and why, where an interviewer is likely to ask
(UDP vs TCP, `htons`, `memcpy` vs `strcpy`, why 1200 bytes, why a 3-second timeout).

- **Publisher**: open socket, then for each frame file: read it, cut it into 1200-byte chunks, put a header on each chunk, `sendto` it.
- **Broker**: one socket, one loop. `recvfrom` gives one packet and the sender's address.
  SUBSCRIBE: save the sender's address and stream in a list. UNSUBSCRIBE: remove them.
  DATA: send the same bytes to everyone in the list for that stream.
  There is no `accept()` and no threads, because UDP has no connections.
- **Subscriber**: send SUBSCRIBE, then loop on `recvfrom`. Each chunk is copied to its place in the frame
  (`chunk_idx * 1200`). When all chunks of a frame are there, save it. After 3 seconds with no data it
  stops. Packets are numbered from 0, so the publisher sent `highest seq + 1` packets; the difference
  from what arrived is the number lost.

## Wireshark

Install it with `brew install --cask wireshark`.

1. Copy the plugin: `mkdir -p ~/.config/wireshark/plugins && cp wireshark/pubsub.lua ~/.config/wireshark/plugins/`, then restart Wireshark.
   (I wrote this plugin but have not been able to test it inside Wireshark. If it fails, you can still read the raw bytes.)
2. Capture on the **Loopback: lo0** interface with capture filter `udp port 9090`.
3. Run the three programs.
4. Useful display filters:

| Filter | Shows |
|---|---|
| `udp.port == 9090` | all our traffic |
| `pubsub.type == 3` | only DATA packets |
| `pubsub.type == 1` | SUBSCRIBE packets |
| `pubsub.frame_id == 7` | all pieces of frame 7 |
| `pubsub.seq >= 40 && pubsub.seq <= 60` | a range of packets |
| `udp.srcport == 9090` | only what the broker sends out |

What to look at:
- Click a DATA packet and expand *User Datagram Protocol* (ports, length, checksum) and *Pub-Sub Packet* (our header).
- Packets from publisher to broker and from broker to subscriber carry the same bytes with different ports. That is the fan-out.
- There is no handshake and no acknowledgement, unlike TCP's SYN / SYN-ACK / ACK.
- Run the broker with loss `10`, sort by `pubsub.seq` and look for the missing numbers.
- Without the GUI: `sudo tcpdump -i lo0 -n -X udp port 9090 -c 20`.

## Debugging with lldb

`make` already builds with `-g`, so you can do:

```bash
lldb ./broker
(lldb) breakpoint set --file broker.cpp --name main
(lldb) run
(lldb) next                    # step line by line
(lldb) frame variable pkt      # look at the packet that just arrived
```

## UDP vs TCP (compared with the old Java version)

| | TCP (Java version) | UDP (this version) |
|---|---|---|
| Connection | accept() and a thread per client | none, one socket |
| Message boundaries | none, needed framing | kept: one sendto = one recvfrom |
| Lost data | resent automatically | stays lost |
| Order | guaranteed | not guaranteed |
| Late joiners | got cached frames | only get live data |

## What to remember

- A frame is only complete if all its chunks arrive. With about 10 chunks per frame and 10% packet loss,
  roughly 65% of frames lose a chunk (1 - 0.9^10).
- Live video uses UDP because waiting for a resend of an old frame is worse than skipping it (TCP's
  head-of-line blocking). This is the idea behind RTP, WebRTC and QUIC.
- Limits of this demo: no encryption or login, no resending of lost chunks, localhost only
  (`127.0.0.1` is written in the code), frames up to 64 KB and ids below 100, subscribers that vanish
  without UNSUBSCRIBE stay in the list, incoming packets are trusted as-is (no size or bounds checks,
  so only our own programs should send to the port), and the subscriber must start before the publisher
  (loss is counted from seq 0).
