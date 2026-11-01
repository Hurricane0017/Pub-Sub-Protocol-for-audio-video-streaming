// protocol.h - the packet format that all three programs share.
//
// Every UDP datagram we send is ONE Packet: a 32-byte header + up to 1200 bytes of data.
// The broker, publisher and subscriber all #include this file, so they agree on the layout.
//
// Numbers go on the network in "network byte order" (big-endian):
//   htons / htonl before sending, ntohs / ntohl after receiving.

#ifndef PROTOCOL_H   // include guard: if PROTOCOL_H is not defined yet, read this file...
#define PROTOCOL_H   // ...and define it, so a second #include of this file is skipped

#include <arpa/inet.h>   // htons/htonl/ntohs/ntohl, inet_pton, and sockaddr_in (an IP address + port)

// --- Constants ---------------------------------------------------------------

const int MAX_DATA = 1200;
// Bytes of video in one packet.
// Why 1200? A network link carries about 1500 bytes per packet (the MTU).
// 1200 data + 32 our header + 8 UDP header + 20 IP header = 1260, which fits,
// so the packet is never split into fragments on the way.

const int HEADER_SIZE = 32;
// Bytes in front of the data. We send HEADER_SIZE + len bytes, not the whole struct,
// so a small last chunk doesn't carry 1200 bytes of empty space.

const int MAX_FRAMES = 100;
// The subscriber has room for frame ids 0..99 (simple fixed-size arrays).

const int MAX_FRAME_BYTES = 65536;
// Biggest frame we support: 64 KB. Our sample frames are about 20 KB.

// --- Packet types ------------------------------------------------------------

enum { SUBSCRIBE = 1, UNSUBSCRIBE = 2, DATA = 3 };
// Names for the numbers 1, 2, 3, so the code says "type == DATA" instead of "type == 3".
// We start at 1 so an all-zero packet is never mistaken for a real one.

// --- The packet --------------------------------------------------------------

struct Packet {
    int   seq;              // bytes 0-3   : packet number 0, 1, 2 ... (a gap means a lost packet)
    int   frame_id;         // bytes 4-7   : which video frame this piece belongs to
    short chunk_idx;        // bytes 8-9   : which piece of the frame this is (0, 1, 2 ...)
    short chunk_total;      // bytes 10-11 : how many pieces the frame was cut into
    short len;              // bytes 12-13 : how many bytes of data are in this packet
    short type;             // bytes 14-15 : SUBSCRIBE, UNSUBSCRIBE or DATA
    char  stream[16];       // bytes 16-31 : stream name, e.g. "movie" (max 15 letters + '\0')
    char  data[MAX_DATA];   // bytes 32+   : a piece of the frame
};
// int = 4 bytes and short = 2 bytes on a Mac. The 4-byte fields come first, then the
// 2-byte ones, so the compiler adds no hidden padding and the header is exactly 32 bytes.
//
// Why a struct and not text like "DATA|5|movie|..."? A struct goes out with one sendto()
// and comes back with one recvfrom(). Text would need code to build it and parse it.

#endif // PROTOCOL_H
