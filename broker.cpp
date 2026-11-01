// broker.cpp - the middleman.
// Publishers send packets to the broker; the broker forwards each one to every
// subscriber of that stream. Publishers and subscribers never talk to each other directly.

#include <sys/socket.h>   // socket(), bind(), recvfrom(), sendto()
#include <cstdlib>        // rand()
#include <iostream>       // cin, cout
#include <string>         // string
#include <vector>         // vector: a list that grows as subscribers join
#include "protocol.h"     // our Packet struct and constants
using namespace std;      // write cout / string / vector instead of std::cout ...

// What we remember about one subscriber.
struct Subscriber {
    sockaddr_in addr;   // their IP + port: where we send packets (UDP has no connection, so this is how we reach them)
    string stream;      // the stream they want
};

int main() {
    int port, loss;
    cout << "Enter the port number: ";
    cin >> port;
    cout << "Packet loss % to simulate (0 for none): ";
    cin >> loss;   // used below to drop packets on purpose, to show loss in Wireshark

    // 1. Create a UDP socket.
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    // AF_INET    = IPv4
    // SOCK_DGRAM = UDP (SOCK_STREAM would be TCP)
    // 0          = default protocol for that type
    // fd is a "file descriptor": a small number the OS uses to identify this socket.

    // 2. Our own address: any network card on this computer, on the given port.
    sockaddr_in me = {};                // = {} sets every byte to 0
    me.sin_family = AF_INET;            // IPv4
    me.sin_addr.s_addr = INADDR_ANY;    // accept packets that arrive on any of our addresses
    me.sin_port = htons(port);          // port in network byte order (big-endian)

    // 3. Attach the socket to that port, so packets sent to the port come to us.
    if (::bind(fd, (sockaddr*)&me, sizeof(me)) < 0) {
        // "::" = the C bind() function, not std::bind (which "using namespace std" brings in).
        // It fails if another program already uses the port (macOS uses 5000 for AirPlay).
        cout << "Could not use port " << port << endl;
        return 1;
    }
    cout << "Broker is running..." << endl;

    vector<Subscriber> subscribers;   // everyone currently subscribed
    Packet pkt;                       // one buffer, reused for every packet

    // 4. Loop forever: receive one packet, then act on its type.
    while (true) {
        sockaddr_in from;                   // recvfrom fills this with the sender's address
        socklen_t fromLen = sizeof(from);   // size of "from"

        int n = recvfrom(fd, &pkt, sizeof(pkt), 0, (sockaddr*)&from, &fromLen);
        // Waits until a packet arrives, copies it into pkt, returns its size in bytes.
        // One recvfrom = exactly one datagram (UDP keeps packet boundaries).

        int type = ntohs(pkt.type);   // network byte order -> our machine's order

        if (type == SUBSCRIBE) {
            Subscriber s;
            s.addr = from;              // remember where to send
            s.stream = pkt.stream;      // and which stream they want
            subscribers.push_back(s);   // add to the list
            cout << "New subscriber for " << pkt.stream << endl;
        }
        else if (type == UNSUBSCRIBE) {
            for (int i = 0; i < subscribers.size(); i++) {
                // In UDP "the same client" = same IP and same port.
                bool sameIp   = subscribers[i].addr.sin_addr.s_addr == from.sin_addr.s_addr;
                bool samePort = subscribers[i].addr.sin_port == from.sin_port;
                if (sameIp && samePort && subscribers[i].stream == pkt.stream) {
                    subscribers.erase(subscribers.begin() + i);   // remove item number i
                    cout << "Subscriber left " << pkt.stream << endl;
                    break;
                }
            }
        }
        else if (type == DATA) {
            // Fan-out: one packet in, one copy out to each subscriber of this stream.
            for (int i = 0; i < subscribers.size(); i++) {
                if (subscribers[i].stream != pkt.stream) continue;   // they watch another stream
                if (rand() % 100 < loss) continue;                   // drop it on purpose (loss% of the time)
                sendto(fd, &pkt, n, 0, (sockaddr*)&subscribers[i].addr, sizeof(subscribers[i].addr));
                // Send the same n bytes we received, unchanged.
            }
        }
    }
    // The broker runs until you press Ctrl+C.
    // One socket and one thread serve everyone: UDP has no connections, so no accept() and no thread per client.
}
