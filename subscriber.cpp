// subscriber.cpp - subscribes to a stream, receives the packets and rebuilds the frames.
// Start it BEFORE the publisher: the broker only forwards live packets, it keeps no history.
// It waits as long as needed for the first packet, then stops after 3 seconds with no data.

#include <sys/socket.h>   // socket(), sendto(), recvfrom(), setsockopt()
#include <sys/stat.h>     // mkdir()
#include <cstdio>         // snprintf()
#include <cstring>        // strncpy(), memcpy()
#include <fstream>        // ofstream: write a file
#include <iostream>       // cin, cout
#include <string>         // string
#include "protocol.h"     // our Packet struct and constants
using namespace std;

// Where the frames are rebuilt: frames[5] holds frame number 5.
// 100 x 64 KB = 6.4 MB, so it is global (outside main) instead of a local array.
// Global variables start at 0, which the counters below rely on.
char frames[MAX_FRAMES][MAX_FRAME_BYTES];
int chunksGot[MAX_FRAMES];     // chunks received so far, per frame
int chunksTotal[MAX_FRAMES];   // chunks the frame has in total (0 = never seen this frame)
int frameSize[MAX_FRAMES];     // bytes received so far, per frame

int main() {
    int port;
    string streamName;
    cout << "Enter the port number: ";
    cin >> port;
    cout << "Enter the stream name: ";
    cin >> streamName;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);              // IPv4 + UDP
    sockaddr_in broker = {};
    broker.sin_family = AF_INET;
    broker.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &broker.sin_addr);    // the broker runs on this computer
    // No bind() here: on the first sendto() the OS gives us a free port automatically,
    // and the broker learns it from recvfrom(). Only the server needs a fixed, known port.

    // Ask for the stream: a packet with just a header, type SUBSCRIBE.
    Packet request = {};
    request.type = htons(SUBSCRIBE);
    strncpy(request.stream, streamName.c_str(), 15);
    sendto(fd, &request, HEADER_SIZE, 0, (sockaddr*)&broker, sizeof(broker));
    cout << "Subscribed to " << streamName << ". Start the publisher now." << endl;

    mkdir("received", 0755);   // output folder (does nothing if it already exists)

    int received = 0;     // packets received
    int lastSeq = 0;      // highest packet number seen
    int framesDone = 0;   // frames saved
    Packet pkt;

    while (true) {
        int n = recvfrom(fd, &pkt, sizeof(pkt), 0, NULL, NULL);   // NULL, NULL: we don't need the sender's address
        if (n < 0) break;   // the 3-second timeout ran out: the stream is over

        if (received == 0) {
            // First packet: from now on, give up if nothing arrives for 3 seconds.
            // Why? UDP never tells us "the sender is finished" (TCP would close the connection).
            // We set it only now so we can wait as long as needed for the publisher to start.
            timeval timeout = {3, 0};   // {seconds, microseconds}
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        }
        received++;

        // Read the header, converting each number back from network byte order.
        int seq   = ntohl(pkt.seq);
        int id    = ntohl(pkt.frame_id);
        int idx   = ntohs(pkt.chunk_idx);
        int total = ntohs(pkt.chunk_total);
        int len   = ntohs(pkt.len);

        if (seq > lastSeq) lastSeq = seq;   // "if", because UDP can deliver packets out of order

        // Put the chunk in its place: chunk idx starts at byte idx * 1200 of the frame.
        // So chunks can arrive in any order and still land in the right spot.
        memcpy(frames[id] + idx * MAX_DATA, pkt.data, len);
        chunksTotal[id] = total;
        chunksGot[id]++;
        frameSize[id] += len;   // add up real lengths, because the last chunk is shorter

        // All chunks here? Save the frame as a file.
        if (chunksGot[id] == total) {
            char name[64];
            snprintf(name, sizeof(name), "received/frame_%03d.png", id);   // e.g. received/frame_007.png
            ofstream out(name, ios::binary);
            out.write(frames[id], frameSize[id]);   // file closes by itself at the end of this block
            framesDone++;
            cout << "Frame " << id << " complete" << endl;
        }
        // If a chunk was lost, chunksGot never reaches total and the frame is never saved.
        // We never ask for a resend: that is the big difference from TCP.
    }

    // Tell the broker we are leaving. Same request packet, new type.
    request.type = htons(UNSUBSCRIBE);
    sendto(fd, &request, HEADER_SIZE, 0, (sockaddr*)&broker, sizeof(broker));

    // Report. The publisher numbers packets from 0, so it sent lastSeq + 1 packets
    // (as far as we can tell: if the very last packets were lost, we can't know about them).
    int expected = lastSeq + 1;
    int framesSeen = 0;
    for (int i = 0; i < MAX_FRAMES; i++)
        if (chunksTotal[i] > 0) framesSeen++;   // at least one chunk of frame i arrived

    cout << endl;
    cout << "Packets: got " << received << " of " << expected << ", lost " << expected - received << endl;
    cout << "Frames: " << framesDone << " complete out of " << framesSeen << endl;
}
