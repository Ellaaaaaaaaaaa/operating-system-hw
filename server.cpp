// server.cpp
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
#include <thread> // For multi-threading (or <sys/wait.h> for multi-processing)

const int PORT = 8080;
const int BUFFER_SIZE = 4096;

// Helper function to create directories recursively
bool create_directories(const std::string& path) {
    // Implementation as above
    std::string current_path;
    std::string temp_path = path;
    if (temp_path.back() == '/') {
        temp_path.pop_back(); // Remove trailing slash
    }

    std::size_t prev_pos = 0;
    std::size_t current_pos;

    while ((current_pos = temp_path.find('/', prev_pos)) != std::string::npos) {
        current_path += temp_path.substr(prev_pos, current_pos - prev_pos);
        if (!current_path.empty()) {
            if (mkdir(current_path.c_str(), 0755) == -1 && errno != EEXIST) {
                std::cerr << "Error creating directory: " << current_path << " - " << strerror(errno) << std::endl;
                return false;
            }
        }
        current_path += '/';
        prev_pos = current_pos + 1;
    }

    // Create the final directory
    if (!temp_path.empty()) {
        if (mkdir(temp_path.c_str(), 0755) == -1 && errno != EEXIST) {
            std::cerr << "Error creating directory: " << temp_path << " - " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}

// Helper function to get file modification time
time_t get_file_mtime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0; // Return 0 or handle error
}


void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    std::string sync_root_dir = "sync_root"; // Directory where synced files will be stored

    // Ensure the sync root directory exists
    if (!create_directories(sync_root_dir)) {
        std::cerr << "Failed to create sync root directory: " << sync_root_dir << std::endl;
        close(client_socket);
        return;
    }

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "Client disconnected or error." << std::endl;
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        std::string command(buffer);
        std::cout << "Received command: " << command << std::endl;

        if (command.rfind("CREATE_DIR", 0) == 0) { // Command starts with CREATE_DIR
            std::string path = command.substr(strlen("CREATE_DIR ") + 1);
            std::string full_path = sync_root_dir + "/" + path;
            if (create_directories(full_path)) {
                send(client_socket, "ACK", 3, 0);
            } else {
                send(client_socket, "NACK", 4, 0);
            }
        } else if (command.rfind("FILE_METADATA", 0) == 0) {
            // Format: FILE_METADATA <relative_path> <size> <mtime>
            std::string path_info = command.substr(strlen("FILE_METADATA ") + 1);
            size_t first_space = path_info.find(' ');
            std::string relative_path = path_info.substr(0, first_space);
            std::string size_mtime_str = path_info.substr(first_space + 1);

            size_t second_space = size_mtime_str.find(' ');
            long long remote_size = std::stoll(size_mtime_str.substr(0, second_space));
            time_t remote_mtime = std::stoll(size_mtime_str.substr(second_space + 1));

            std::string full_path = sync_root_dir + "/" + relative_path;

            struct stat st;
            bool file_exists = (stat(full_path.c_str(), &st) == 0);
            
            if (!file_exists || remote_mtime > st.st_mtime || remote_size != st.st_size) {
                // File needs to be updated or created
                send(client_socket, "RECEIVE_FILE", 12, 0);
            } else {
                send(client_socket, "SKIP_FILE", 9, 0);
            }

        } else if (command.rfind("FILE_CONTENT", 0) == 0) {
            // Format: FILE_CONTENT <relative_path>
            std::string relative_path = command.substr(strlen("FILE_CONTENT ") + 1);
            std::string full_path = sync_root_dir + "/" + relative_path;

            std::ofstream ofs(full_path, std::ios::binary);
            if (!ofs.is_open()) {
                std::cerr << "Error opening file for writing: " << full_path << std::endl;
                send(client_socket, "NACK", 4, 0);
                continue;
            }
            send(client_socket, "ACK", 3, 0); // Acknowledge readiness to receive file content

            long long file_bytes_received = 0;
            char file_buffer[BUFFER_SIZE];
            
            // Loop to receive actual file data
            while (true) {
                bytes_received = recv(client_socket, file_buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    std::cerr << "Error receiving file data or client disconnected." << std::endl;
                    break;
                }
                
                // Check for END_FILE_TRANSFER signal
                if (bytes_received == strlen("END_FILE_TRANSFER") && 
                    strncmp(file_buffer, "END_FILE_TRANSFER", strlen("END_FILE_TRANSFER")) == 0) {
                    break; 
                }

                ofs.write(file_buffer, bytes_received);
                file_bytes_received += bytes_received;
            }
            ofs.close();
            std::cout << "Received file: " << full_path << ", size: " << file_bytes_received << " bytes." << std::endl;
            send(client_socket, "ACK", 3, 0); // Acknowledge file reception

        } else if (command == "QUIT") {
            std::cout << "Client requested to quit." << std::endl;
            break;
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            send(client_socket, "NACK", 4, 0);
        }
    }
    close(client_socket);
}

int main(int argc, char* argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    address.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) { // Max 10 pending connections
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        std::cout << "New connection from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;

        // Use a new thread for each client
        // For APUE, you might be expected to use fork() for multi-processing.
        // If using fork(), remember to close new_socket in the parent process.
        std::thread client_thread(handle_client, new_socket);
        client_thread.detach(); // Detach the thread to run independently
    }

    close(server_fd);
    return 0;
}