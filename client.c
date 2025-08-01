#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PATH_MAX 4096
#define BUFFER_SIZE 4096
#define MAX_FILE_NAME 256
int server_sock;

void receive_updates(int sock, const char *local_directory) {
    while (1) {
        char buffer[BUFFER_SIZE];
        char file_ka_naam[MAX_FILE_NAME];
        char file_size_str[20];
        long file_size = 0, received_size = 0;
        int bytes_received, i = 0, j = 0;

        // Step 1: Receive initial chunk ensuring complete metadata
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive metadata");
            return;
        }
        buffer[bytes_received] = '\0'; // Null-terminate

        // Step 2: Extract filename
        while (buffer[i] != '#' && buffer[i] != '@' && buffer[i] != '\0') {
            file_ka_naam[i] = buffer[i];
            i++;
        }
        file_ka_naam[i] = '\0';

        // Construct full file path
        char full_path[MAX_FILE_NAME + 512];
        snprintf(full_path, sizeof(full_path), "%s/%s", local_directory, file_ka_naam);
        printf("Receiving: %s\n", full_path);

        // Step 3: Handle delete request
        if (buffer[i] == '@') {
            char command[1024];
            snprintf(command, sizeof(command), "rm -rf \"%s\"", full_path);
            system(command);
            continue;
        }

        // Step 4: Extract file size
        i++; // Skip '#'
        j = 0;
        while (buffer[i] != '#' && buffer[i] != '\0') {
            file_size_str[j++] = buffer[i++];
        }
        file_size_str[j] = '\0';
        file_size = atol(file_size_str);

        i++; // Skip '#'
        int is_dir = (buffer[i] == '1');
        i += 2;

        printf("File: %s (Size: %ld bytes)\n", file_ka_naam, file_size);

        // Step 5: Handle directory creation
        if (is_dir) {
            char command[1024];
            snprintf(command, sizeof(command), "mkdir -p \"%s\"", full_path);
            system(command);
            continue;
        }
        else {
            // Extract parent directory
            char parent_dir[1024];
            strncpy(parent_dir, full_path, sizeof(parent_dir));
            parent_dir[sizeof(parent_dir) - 1] = '\0'; // Ensure null-termination
        
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash) {
                *last_slash = '\0'; // Remove filename to get the parent directory
                char command[1024];
                snprintf(command, sizeof(command), "mkdir -p \"%s\"", parent_dir);
                system(command);
            }
        }

        // Step 6: Open file for writing
        FILE *file = fopen(full_path, "wb");
        if (!file) {
            perror("Failed to open file");
            return;
        }

        // Step 7: Handle first chunk correctly
        received_size = bytes_received - i;
        if (received_size > 0) {
            fwrite(buffer + i, 1, received_size, file);
        }

        // Step 8: Receive remaining file contents
        while (received_size < file_size) {
            bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                perror("Error receiving file data");
                break;
            }
            fwrite(buffer, 1, bytes_received, file);
            received_size += bytes_received;
        }

        fclose(file);

        // Ensure complete file reception
        if (received_size == file_size) {
            printf("Received successfully: %s\n", full_path);
        } else {
            printf("Transfer incomplete. Expected: %ld, Got: %ld\n", file_size, received_size);
        }
    }
}


void send_ignore_lst(const char *filepath){
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        perror("Failed to get file size");
        close(file_fd);
        return;
    }
    off_t file_size = file_stat.st_size;

    char metadata[PATH_MAX];
    snprintf(metadata, sizeof(metadata), "%ld#", file_size);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

        // Send metadata
        if (send(server_sock, metadata, strlen(metadata), 0) < 0) {
            perror("send metadata");
            return;
        }

        // Send file contents
        lseek(file_fd, 0, SEEK_SET);
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            if (send(server_sock, buffer, bytes_read, 0) < 0) {
                perror("send file data");
            }
        }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ./syncclient <server_ip> <port> <local_directory> <ignore_list_file>\n");
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *local_directory = argv[3];
    const char *ignore_list_file = argv[4];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    server_sock = sock;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    send_ignore_lst(ignore_list_file);

    // FILE *file = fopen(ignore_list_file, "r");
    // if (!file) {
    //     perror("Failed to open ignore list file");
    //     return 1;
    // }
    
    // char ignore_list[BUFFER_SIZE] = "";
    // char ext[50];
    // while (fscanf(file, "%49s", ext) != EOF) {
    //     strcat(ignore_list, ext);
    //     strcat(ignore_list, ",");
    // }
    // fclose(file);
    // send(sock, ignore_list, strlen(ignore_list), 0);
    receive_updates(sock, local_directory);
    close(sock);
    return 0;
}
