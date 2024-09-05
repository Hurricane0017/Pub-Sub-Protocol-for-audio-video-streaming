import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashMap;
import java.util.Map;
import java.util.Scanner;
import java.util.concurrent.ConcurrentLinkedQueue;

class StreamVideo {
    
    final Integer publisher;
    final ConcurrentLinkedQueue<OutputStream> outputStreams;
    final byte[][] headers;
    
    StreamVideo(String name, Integer publisher, int totalFiles) {
        this.publisher = publisher;
        outputStreams = new ConcurrentLinkedQueue<>();
        this.headers = new byte[totalFiles][500*1024];
    }

}

public class Broker {
    
    public static void main(String[] args) throws IOException {
        Scanner scanner = new Scanner(System.in);
        int portNumber;
        System.out.print("Enter the port number: ");
        portNumber = scanner.nextInt();
        new Broker(portNumber);
    }
    public void addSubscriber(int clientID, BrokerThread subscriber) {
        subscribers.put(clientID, subscriber);
    }
    public boolean containsSubscriber(int port) {
        return subscribers.containsKey(port);
    }
    public int indexOfSubscriber(int port) {
        if (subscribers.containsKey(port)) {
            int index = 0;
            for (int key : subscribers.keySet()) {
                if (key == port) {
                    return index;
                }
                index++;
            }
        }
        return -1;
    }        
    public void removeSubscriber(int port) {
        subscribers.remove(port);
    }      
    public Map<Integer, BrokerThread> getSubscribers() {
        return subscribers;
    }
    public Map<Integer, BrokerThread> getPublishers() {
        return publishers;
    }
    public void clientClose(String streamName, int clientPort) {
        if (subscribers.containsKey(clientPort)) {
            BrokerThread close = subscribers.get(clientPort);
            try {
                close.socket.close();
                subscribers.remove(clientPort);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
    
    
    

    private final Map<Integer, BrokerThread> clients = new HashMap<>();
    private final Map<Integer, BrokerThread> publishers = new HashMap<>();
    private final Map<Integer, BrokerThread> subscribers = new HashMap<>(); // Initialize subscribers map
    protected final Map<String, StreamVideo> streamChannels = new HashMap<>();


    private Broker(int portNumber) {
        try (ServerSocket serverSocket = new ServerSocket(portNumber)) {
            while (true) {
                addNewClient(serverSocket.accept());
            }
        } catch (IOException e) {
            System.err.println("COULDN'T LISTEN ON PORT: " + portNumber);
            System.exit(-1);
        }
    }

    private void addNewClient(Socket socket) {
        System.out.println("NEW CONNECTION: " + socket);
        clients.put(socket.getPort(), new BrokerThread(this, socket, socket.getPort()));
        clients.get(socket.getPort()).start();
    }

    public void addNewSubscriber(int clientID) {
        subscribers.put(clientID, clients.remove(clientID)); // Add the client to the subscribers map
        System.out.println("SUBSCRIBER: " + subscribers.get(clientID).socket + " has entered the building!");
    }

    public void addNewPublisher(int clientID, String streamName, int totalFiles) {
        publishers.put(clientID, clients.remove(clientID));
        StreamVideo stream = new StreamVideo(streamName, clientID, totalFiles);
        streamChannels.put(streamName, stream);
        System.out.println("PUBLISHER: " + publishers.get(clientID).socket + " has entered the building!");
        System.out.println("PUBLISHER: " + publishers.get(clientID).socket + " created stream " + streamName);
    }

    public void handleFrameUpload(String streamName, byte[] frameData) {
        StreamVideo stream = streamChannels.get(streamName);
        if (stream != null) {
            // Send the frame data to all subscribers
            for (OutputStream outputStream : stream.outputStreams) {
                try {
                    outputStream.write(frameData);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

        public void clientClose(int clientID) {
            BrokerThread close = clients.get(clientID);
            if (close != null) {
                try {
                    close.socket.close();
                    clients.remove(clientID);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            // Handle removal from subscribers and publishers if necessary
            // ...
        }
}
