import java.io.*;
import java.net.Socket;
import java.net.UnknownHostException;

public class Publisher {
    public static void main(String[] args) throws IOException {
        // Use port number 54321
        int portNumber = 54323;
        String hostName = "localhost"; // You can change this if needed.

        // Get the stream name from user input
        BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));

        System.out.print("Enter the port number: ");
        portNumber = Integer.parseInt(reader.readLine());

        while(true){
        System.out.print("Enter the stream name: ");
        String streamName = reader.readLine();

        // Prompt the user for the folder path where video frames are located
        System.out.print("Enter the folder path where video frames are located: ");
        String frameFolderPath = reader.readLine();

        // Check if the frame folder exists
        File frameFolder = new File(frameFolderPath);
        if (!frameFolder.exists() || !frameFolder.isDirectory()) {
            System.err.println("Frame folder not found: " + frameFolderPath);
            System.exit(1);
        }

        try (
            Socket socket = new Socket(hostName, portNumber);
            PrintWriter out = new PrintWriter(socket.getOutputStream(), true)
        ) {
            // Send the "publish" command to the server
            File[] frameFiles = frameFolder.listFiles();
            int totalFiles = frameFiles.length;
            out.println("publisher " + streamName + " " + totalFiles);
        
            // Get the socket's output stream
            OutputStream socketOutput = socket.getOutputStream();
        
            // List frame files in the folder
        
            if (frameFiles != null) {
                int totalFrames = frameFiles.length;
                int currentFrame = 0;
                 // Total number of files
                // socketOutput.writeLong(totalFiles); // Send the total file count
                for (File frameFile : frameFiles) {
                    if (frameFile.isFile()) {
                        try (FileInputStream frameData = new FileInputStream(frameFile)) {
                            int bufferSize = 500 * 1024; // Adjust the buffer size as needed (500 KB)
                            byte[]data = new byte[bufferSize];
                            int bytesRead;
                            // int totalBytes = 0;
                            while ((bytesRead = frameData.read(data)) != -1) {
                                socketOutput.write(data, 0, bytesRead);
                                socketOutput.flush();
                                // totalBytes += bytesRead;
                            }
                            // System.out.println("Frame length " + totalBytes);
                            // Increase the current frame count
                            currentFrame++;
                            // Print progress
                            System.out.println("Uploaded frame " + currentFrame + " out of " + totalFrames);
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
                }
                socketOutput.flush();
                socketOutput.close();
            }
        } catch (UnknownHostException e) {
            System.err.println("Host unknown " + hostName);
            System.exit(1);
        } catch (IOException e) {
            System.err.println("I/O retrieve not possible " + hostName);
            System.exit(1);
        }
        
    }
}
}
