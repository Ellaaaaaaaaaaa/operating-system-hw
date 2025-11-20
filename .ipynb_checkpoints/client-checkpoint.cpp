// client.cpp
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <algorithm>


const int PORT = 8080;
const int BUFFER_SIZE = 4096;

// Helper function to get file modification time
time_t get_file_mtime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

// Function to send data and receive acknowledgment
bool send_command_and_wait_ack(int sock, const std::string& command) {
    send(sock, command.c_str(), command.length(), 0);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        std::cerr << "Error receiving ACK or server disconnected." << std::endl;
        return false;
    }
    std::string response(buffer);
    return response == "ACK" || response == "RECEIVE_FILE" || response == "SKIP_FILE"; // Adapt to server responses
}


// Recursive function to sync a directory
void sync_directory(int sock, const std::string& local_path, const std::string& remote_base_path) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(local_path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") {
                continue;
            }

            std::string full_local_path = local_path + "/" + name;
            std::string full_remote_path = remote_base_path + "/" + name;
            struct stat st;

            if (stat(full_local_path.c_str(), &st) == -1) {
                std::cerr << "Error stating file: " << full_local_path << std::endl;
                continue;
            }

            if (S_ISDIR(st.st_mode)) { // It's a directory
                std::string command = "CREATE_DIR " + full_remote_path;
                std::cout << "Sending: " << command << std::endl;
                send(sock, command.c_str(), command.length(), 0);
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                recv(sock, buffer, BUFFER_SIZE - 1, 0); // Receive ACK/NACK
                std::string response(buffer);
                if (response == "ACK") {
                    std::cout << "Directory created on server: " << full_remote_path << std::endl;
                } else {
                    std::cerr << "Failed to create directory on server: " << full_remote_path << std::endl;
                }
                sync_directory(sock, full_local_path, full_remote_path); // Recurse
            } else if (S_ISREG(st.st_mode)) { // It's a regular file
                long long file_size = st.st_size;
                time_t file_mtime = st.st_mtime;

                std::string metadata_cmd = "FILE_METADATA " + full_remote_path + " " +
                                           std::to_string(file_size) + " " + std::to_string(file_mtime);
                
                std::cout << "Sending: " << metadata_cmd << std::endl;
                send(sock, metadata_cmd.c_str(), metadata_cmd.length(), 0);

                char response_buffer[BUFFER_SIZE];
                memset(response_buffer, 0, BUFFER_SIZE);
                recv(sock, response_buffer, BUFFER_SIZE - 1, 0); // Wait for server's decision
                std::string server_response(response_buffer);
                
                if (server_response == "RECEIVE_FILE") {
                    std::cout << "Server needs file: " << full_local_path << ". Sending content..." << std::endl;

                    std::string file_content_cmd = "FILE_CONTENT " + full_remote_path;
                    send(sock, file_content_cmd.c_str(), file_content_cmd.length(), 0);

                    memset(response_buffer, 0, BUFFER_SIZE);
                    recv(sock, response_buffer, BUFFER_SIZE - 1, 0); // Wait for server's ACK to start file transfer
                    if (std::string(response_buffer) != "ACK") {
                        std::cerr << "Server not ready to receive file content for: " << full_local_path << std::endl;
                        continue;
                    }

                    std::ifstream ifs(full_local_path, std::ios::binary);
                    if (!ifs.is_open()) {
                        std::cerr << "Error opening local file for reading: " << full_local_path << std::endl;
                        continue;
                    }

                    char file_buffer[BUFFER_SIZE];
                    while (!ifs.eof()) {
                        ifs.read(file_buffer, BUFFER_SIZE);
                        send(sock, file_buffer, ifs.gcount(), 0);
                    }
                    ifs.close();

                    // Send end-of-file signal
                    send(sock, "END_FILE_TRANSFER", strlen("END_FILE_TRANSFER"), 0);
                    
                    memset(response_buffer, 0, BUFFER_SIZE);
                    recv(sock, response_buffer, BUFFER_SIZE - 1, 0); // Wait for final ACK for file reception
                    if (std::string(response_buffer) == "ACK") {
                        std::cout << "Successfully sent file: " << full_local_path << std::endl;
                    } else {
                        std::cerr << "Server reported error receiving file: " << full_local_path << std::endl;
                    }

                } else if (server_response == "SKIP_FILE") {
                    std::cout << "Server has up-to-date file, skipping: " << full_local_path << std::endl;
                } else {
                    std::cerr << "Unexpected server response for metadata: " << server_response << std::endl;
                }
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Could not open directory: " << local_path << std::endl;
    }
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <source_directory>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string server_ip = argv[1];
    std::string source_directory = argv[2];

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return EXIT_FAILURE;
    }

    std::cout << "Connected to server " << server_ip << ":" << PORT << std::endl;

    // Start syncing from the source directory, treat it as the root for remote
    std::string remote_base_name = source_directory.substr(source_directory.find_last_of('/') + 1);
    std::string initial_remote_path = remote_base_name.empty() ? source_directory : remote_base_name;

    // Send initial command to create the root sync directory on the server
    std::string create_root_cmd = "CREATE_DIR " + initial_remote_path;
    send(sock, create_root_cmd.c_str(), create_root_cmd.length(), 0);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (std::string(buffer) != "ACK") {
        std::cerr << "Failed to create root sync directory on server: " << initial_remote_path << std::endl;
        close(sock);
        return EXIT_FAILURE;
    }
    std::cout << "Root sync directory created on server: " << initial_remote_path << std::endl;


    sync_directory(sock, source_directory, initial_remote_path);

    // Send quit command to server
    send(sock, "QUIT", 4, 0);

    close(sock);
    return 0;
}