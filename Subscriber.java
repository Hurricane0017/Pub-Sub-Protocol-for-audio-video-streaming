import java.io.*;
import java.net.Socket;
import java.net.UnknownHostException;

public class Subscriber {
    public static void main(String[] args) throws IOException {
        // Use port number 54321
        int portNumber = 54323;
        String hostName = "localhost"; // You can change this if needed.

        // Create a BufferedReader to read user input
        BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));

        // Ask for the host name
        // System.out.print("Enter the host name: ");
        hostName = "localhost";

        // Ask for the port number
        System.out.print("Enter the port number: ");
        portNumber = Integer.parseInt(reader.readLine());

        try (
            Socket socket = new Socket(hostName, portNumber);
            PrintWriter outputToServer = new PrintWriter(socket.getOutputStream(), true);
            BufferedReader serverInput = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            BufferedReader terminalInput = new BufferedReader(new InputStreamReader(System.in))
        ) {
            outputToServer.println("subscriber");
            String fromServer;
            String fromUser;

            try {
                System.out.print("Type \"list\" to see the Streams\n");
                while (true) {
                    fromUser = terminalInput.readLine();
                    String input = fromUser;
                    String[] inputParts = input.split(" ");
                    if (fromUser.equals("list")) {
                        outputToServer.println(fromUser);
                        while ((fromServer = serverInput.readLine()) != null) {
                            if (fromServer.equals("...")) {
                                break;
                            }
                            System.out.println(fromServer);
                        }
                    } else if (inputParts[0].equals("subscribe")) {
                        outputToServer.println(fromUser);
                        System.out.println("Entered subscribe mode");
                        fromServer = serverInput.readLine();
                        int totalFiles = Integer.parseInt(fromServer);
                        BufferedInputStream in = new BufferedInputStream(socket.getInputStream());
                        int bufferSize = 500*1024; // 1 MB = 1024 KB, 1 KB = 1024 bytes
                        byte[][] data = new byte[totalFiles][bufferSize];
                        int totalFramesReceived = 0;

                        int bytesRead;
                        
                        for(int i = 0; i<totalFiles; ++i){
                            bytesRead = in.read(data[i]);
                            totalFramesReceived++;
                            System.out.printf("Received frame %d of %d\n", totalFramesReceived, totalFiles);
                        }
                        System.out.println("Subscription ended. Type \"list\" to see the Streams.");
                        // socket.getOutputStream().close();
                    } else if (inputParts[0].equals("unsubscribe")) {
                        outputToServer.println(fromUser);
                        System.out.println("Unsubscribed from the current topic.");
                    } else {
                        System.out.println("Invalid command. Type 'list' to see options.");
                    }
                }
            } catch (IOException e) {
                System.err.println("Couldn't get I/O for the connection to " + hostName);
                outputToServer.println("unsubscribe");
            }
        } catch (UnknownHostException e) {
            System.err.println("Don't know about host " + hostName);
            System.exit(1);
        }
    }
}
