import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;

public class BrokerThread extends Thread {
    Socket socket = null;
    private Broker broker = null;
    private final int port;

    BrokerThread(Broker broker, Socket socket, int port) {
        super("BrokerThread");
        this.socket = socket;
        this.broker = broker;
        this.port = port;
    }

    public void run() {
        String stream_name = "";
        int totalFiles = 0;
        try (
            PrintWriter streamOut = new PrintWriter(socket.getOutputStream(), true);
            BufferedReader streamIn = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            BufferedInputStream in = new BufferedInputStream(socket.getInputStream())
        ) {
            String input = streamIn.readLine();
            String splitInput[] = input.split(" ");
            if (splitInput.length == 1) {
                // Input doesn't contain any space, treat it as a command or argument
                stream_name = splitInput[0];
                // Handle the command or argument accordingly
            } 
            else {
                stream_name = splitInput[1];
                totalFiles = Integer.parseInt(splitInput[2]);
            }
            
            // publisher
            Map<String, StreamVideo> streamChannels = broker.streamChannels;
            
            try {
                if (splitInput[0].equals("publisher")) {
                    broker.addNewPublisher(port, stream_name, totalFiles);
                    StreamVideo stream = streamChannels.get(stream_name);
                    System.out.println("Total frames Recived " + totalFiles);

                    for(int p = 0; p<totalFiles; ++p){
                        int frameSize = 500 * 1024;
                        byte[] frameData = new byte[frameSize];
                        in.read(frameData);
                        // System.out.println("Frame length " + frameData.length);

                        byte[][] headers = stream.headers;
                        // Store the received frame in the current header
                        System.arraycopy(frameData, 0, headers[p], 0, frameSize);

                        ConcurrentLinkedQueue<OutputStream> subscribers = stream.outputStreams;
                        for (OutputStream clientOutputStream : subscribers) {
                            byte[] frameDataClone = frameData.clone();
                            clientOutputStream.write(frameDataClone, 0, frameDataClone.length);
                            clientOutputStream.flush();
                        }
                    }
                }
            } catch (IOException e) {
                broker.clientClose(port);
            }

            // subscriber
            try {
                if (input.equals("subscriber")) {
                    broker.addNewSubscriber(port);
                    while (true) {
                        input = streamIn.readLine();
                        // streamOut.println("22222");
                        splitInput = input.split(" ");
                        Map<Integer, BrokerThread> subscribers = broker.getSubscribers();
                        if (input.equals("list")) {
                            streamOut.println("We have all sorts of products. Let me show you.");
                            for (String name_stream : streamChannels.keySet()) {
                                int publisherID = streamChannels.get(name_stream).publisher;
                                BrokerThread publisher = broker.getPublishers().get(publisherID);
                                // System.out.println("Product: " + name_stream + " | " + publisher);
                                streamOut.println("Product: " + name_stream + " | " + publisher);
                            }
                            streamOut.println("Number of subscribers: " + (subscribers.size() - 1));
                            streamOut.println("...");
                        } else if (splitInput[0].equals("subscribe")) {
                            String product = splitInput[1];
                            stream_name = product;
                            BufferedOutputStream out;
                            StreamVideo channels = streamChannels.get(product);
                            Socket subscriberSocket = subscribers.get(port).socket;
                            System.out.println("Subscriber " + subscriberSocket + " is watching this product --> " + product);
                            OutputStream outputStreamSubscribers = subscriberSocket.getOutputStream();
                            streamOut.println(channels.headers.length);
                            for (int frameIndex = 0; frameIndex < channels.headers.length; frameIndex++) {
                                byte[] data = channels.headers[frameIndex];
                                out = new BufferedOutputStream(outputStreamSubscribers);
                                out.write(data);
                                out.flush();

                                // Optionally, you can print a message indicating which frame is being sent
                                // System.out.println("Sent frame " + (frameIndex + 1) + " of " + channels.headers.length);
                            }
                            broker.addSubscriber(port, this);
                            channels.outputStreams.add(new BufferedOutputStream(outputStreamSubscribers));
                        }
                        else if (splitInput[0].equals("unsubscribe")) {
                            String product = splitInput[1];
                            stream_name = product;
                            StreamVideo channels = streamChannels.get(product);
                            Socket subscriberSocket = subscribers.get(port).socket;
                            
                            if (broker.containsSubscriber(port)) {
                                int index = broker.indexOfSubscriber(port);
                                broker.removeSubscriber(index);
                                channels.outputStreams.remove(index);
                                System.out.println("Subscriber " + subscriberSocket + " has unsubscribed from product --> " + product);
                            } else {
                                System.out.println("Subscriber " + subscriberSocket + " is not subscribed to product --> " + product);
                            }
                        }
                    }
                }
            } catch (IOException e) {
                broker.clientClose(stream_name, port);
            }
        } catch (IOException e) {
            broker.clientClose(stream_name, port);
        }
    }

    public static void main(String[] args) throws IOException {
        // The BrokerThread class is not meant to be run as a standalone application.
        System.err.println("This class is not meant to be run as a standalone application.");
        System.exit(1);
    }
}
