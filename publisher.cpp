// publisher.cpp - reads frame001.png, frame002.png ... from a folder and sends them to the broker.
// A frame (about 20 KB) is bigger than one packet (1200 bytes of data), so each frame is
// cut into pieces ("chunks") and each chunk is sent as its own packet.

#include <sys/socket.h>   // socket(), sendto()
#include <unistd.h>       // usleep(): pause for some microseconds
#include <cstdio>         // snprintf(): build the file name
#include <cstring>        // strncpy(), memcpy()
#include <fstream>        // ifstream: read a file
#include <iostream>       // cin, cout
#include <string>         // string
#include "protocol.h"     // our Packet struct and constants
using namespace std;

int main() {
    int port;
    string streamName, folder;
    cout << "Enter the port number: ";
    cin >> port;
    cout << "Enter the stream name: ";
    cin >> streamName;
    cout << "Enter the folder where the frames are: ";
    cin >> folder;

    // UDP socket + the broker's address. No connect step: UDP has no connection.
    int fd = socket(AF_INET, SOCK_DGRAM, 0);              // IPv4 + UDP
    sockaddr_in broker = {};                              // = {} sets every byte to 0
    broker.sin_family = AF_INET;                          // IPv4
    broker.sin_port = htons(port);                        // port in network byte order
    inet_pton(AF_INET, "127.0.0.1", &broker.sin_addr);    // text IP -> 4 bytes; 127.0.0.1 = this computer

    int seq = 0;   // packet counter: 0, 1, 2 ... The subscriber uses gaps in it to count lost packets.

    for (int frame = 1; frame < MAX_FRAMES; frame++) {
        // File name, e.g. "frames/frame007.png". %03d = number padded to 3 digits (7 -> 007).
        char path[256];
        snprintf(path, sizeof(path), "%s/frame%03d.png", folder.c_str(), frame);

        ifstream file(path, ios::binary);   // binary: read the bytes exactly as they are
        if (!file) break;                   // no such file -> no more frames, stop

        char bytes[MAX_FRAME_BYTES];         // the whole frame
        file.read(bytes, sizeof(bytes));     // read the file (it is smaller than 64 KB)
        int size = file.gcount();            // how many bytes were actually read

        int chunks = (size + MAX_DATA - 1) / MAX_DATA;
        // Number of 1200-byte chunks, rounded UP: 20284 bytes -> 17 chunks (16 full + 1 partial).

        for (int c = 0; c < chunks; c++) {
            int len = size - c * MAX_DATA;          // bytes left from the start of this chunk
            if (len > MAX_DATA) len = MAX_DATA;     // all chunks are 1200 bytes, except maybe the last

            Packet pkt = {};                        // start with all zeros
            pkt.seq = htonl(seq++);                 // htonl = 4-byte version of htons; seq++ = use, then add 1
            pkt.frame_id = htonl(frame);
            pkt.chunk_idx = htons(c);
            pkt.chunk_total = htons(chunks);
            pkt.len = htons(len);
            pkt.type = htons(DATA);
            strncpy(pkt.stream, streamName.c_str(), 15);   // at most 15 letters; byte 16 stays '\0'
            memcpy(pkt.data, bytes + c * MAX_DATA, len);
            // Copy this chunk's bytes. memcpy, not strcpy: strcpy stops at the first 0 byte,
            // and image data is full of 0 bytes.

            sendto(fd, &pkt, HEADER_SIZE + len, 0, (sockaddr*)&broker, sizeof(broker));
            // One datagram: header + len bytes. No handshake before, no acknowledgement after.

            usleep(1000);
            // Wait 1 ms between packets. Without it we send them so fast the receiver's
            // buffer can overflow, and the OS silently drops packets.
        }
        cout << "Sent frame " << frame << " in " << chunks << " packets" << endl;
    }
    cout << "Done, sent " << seq << " packets" << endl;
}
