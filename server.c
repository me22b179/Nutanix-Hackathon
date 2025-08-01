#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>

#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define MAX_EVENT_MONITOR 2048
#define NAME_LEN 32
#define MONITOR_EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_SIZE MAX_EVENT_MONITOR * (MONITOR_EVENT_SIZE + NAME_LEN)

#define MAX_WATCHES 1024
#define BUF_SIZE 1024
#define MAX_CLIENTS 20
int max_clients;

int arr[MAX_CLIENTS]={0};

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int clients[MAX_CLIENTS];
int client_count = 0;

typedef struct {
    int wd;
    char path[PATH_MAX];
} WatchEntry;

int fd;
WatchEntry watch_list[MAX_WATCHES];
int watch_count = 0;

int syncDir;

typedef struct {
    int client_id;
    long file_size;
    char file_metadata[BUFFER_SIZE];
} ClientFileData;

ClientFileData client_files[MAX_CLIENTS]; 


void remove_client(int client_sock) {
    pthread_mutex_lock(&clients_mutex);
    int client_no = -1;

    // Find the client_no for this client_sock
    for (int i = 0; i < max_clients; i++) {
        if (clients[i] == client_sock) {
            client_no = i;
            break;
        }
    }

    if (client_no != -1) {
        printf("Client %d disconnected\n", client_no);

        // Close the socket
        close(client_sock);
        
        // Free up the slot
        clients[client_no] = 0;
        arr[client_no] = 0;

        // Clear the stored ignore list
        memset(client_files[client_no].file_metadata, 0, sizeof(client_files[client_no].file_metadata));

        client_count--;
        printf("total client count remaining: %d\n", client_count);
    }

    pthread_mutex_unlock(&clients_mutex);
}


// Function to add a client to the list
void handle_client(int client_sock) {
    printf("client sock: %d\n", client_sock);
    pthread_mutex_lock(&clients_mutex);
    int client_no;
    if (client_count < max_clients) {
        for(int i = 0; i < max_clients; i++){
            if(arr[i]==0){
                arr[i]=1;
                client_no = i;
                client_count++;
                clients[i] = client_sock;
                break;
            }
        }
        
        printf("New client connected\n");
    } else {
        printf("Max clients reached. Connection rejected.\n");
        close(client_sock);
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    pthread_mutex_unlock(&clients_mutex);


    char buffer[BUFFER_SIZE];
    long file_size;
    char file_metadata[BUFFER_SIZE];
    int bytes_received;

    // Receive file_size#file_metadata
    bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive data");
        close(client_sock);
        return NULL;
    }
    buffer[bytes_received] = '\0';
    // printf("bytes rcvd: %d\n", bytes_received);

    // Parse file_size
    int i = 0, j = 0;
    char file_size_str[20];
    while (buffer[i] != '#' && buffer[i] != '\0') {
        file_size_str[j++] = buffer[i++];
    }
    file_size_str[j] = '\0';
    file_size = atol(file_size_str);
    // printf("buffer: ");
    puts(buffer);


    i++; // Move past '#'

    // Extract file metadata
    // Extract file metadata
    if (buffer[i] != '\0') {
        strncpy(client_files[client_no].file_metadata, buffer + i, sizeof(client_files[client_no].file_metadata) - 1);
        client_files[client_no].file_metadata[sizeof(client_files[client_no].file_metadata) - 1] = '\0'; // Explicit null-termination
    } else {
        strcpy(client_files[client_no].file_metadata, "(empty)");
    }

        // puts(client_files[client_no].file_metadata);
        while (recv(client_sock, buffer, sizeof(buffer), 0) > 0) {
            // Do nothing
        }
    
        // If the client disconnects, clean up
        remove_client(client_sock);

}

int has_ignored_extension(const char *file_ka_naam, const char *metadata) {
    // Find the last occurrence of '.'
    const char *file_ext = strrchr(file_ka_naam, '.');
    if (!file_ext) return 0;  // No extension found
    puts(file_ext);
    // printf("futher search\n");
    // Tokenize metadata (comma-separated extensions)
    puts(metadata);
    char metadata_copy[256];  // Create a copy since strtok modifies the string
    strncpy(metadata_copy, metadata, sizeof(metadata_copy) - 1);
    metadata_copy[sizeof(metadata_copy)-1] = '\0'; // Ensure null-termination
    // puts(metadata_copy);
    char *token = strtok(metadata_copy, ",");
    while (token) {
        puts(token);
        if (strcmp(file_ext, token) == 0) {
            return 1;  // Match found
        }
        token = strtok(NULL, ",");
    }
    return 0;  // No match found
}



// Send file contents to all clients
void send_file_to_clients(const char *filepath, char is_dir) {
    sleep(1);
    int file_fd = open(filepath, O_RDONLY | O_LARGEFILE | O_SYNC);

    // if (file_fd < 0) {
    //     perror("Failed to open file");
    //     return;
    // }

    struct stat file_stat;
    if (stat(filepath, &file_stat) < 0) {
        perror("Failed to get file size");
        // close(file_fd);
        return;
    }
    off_t file_size = file_stat.st_size;

    // if (!S_ISREG(file_stat.st_mode)) {
    //     fprintf(stderr, "Error: Not a regular file\n");
    //     close(file_fd);
    //     return;
    // }
    
    // printf("File descriptor: %d\n", file_fd);
    // printf("File size reported by fstat: %ld bytes\n", file_stat.st_size);

    int len = strlen(filepath) - syncDir;
    char file_ka_naam[len + 1];
    strncpy(file_ka_naam, filepath + syncDir, len);
    file_ka_naam[len] = '\0';

    // Create the metadata string (filename#filesize#)
    char metadata[PATH_MAX];
    snprintf(metadata, sizeof(metadata), "%s#%ld#%c#", file_ka_naam, file_size, is_dir);
    // printf("metadata: ");
    puts(metadata);

    char buffer[BUF_SIZE];
    ssize_t bytes_read;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if(has_ignored_extension(file_ka_naam, client_files[i].file_metadata)){
            continue;
        }
        // else printf("not found\n");

        // Send metadata
        size_t metadata_len = strlen(metadata);

        size_t total_sent = 0;
        ssize_t bytes_sent;

        while (total_sent < metadata_len) {
            bytes_sent = send(clients[i], metadata + total_sent, metadata_len - total_sent, 0);
            if (bytes_sent < 0) {
                perror("send metadata");
                return;  // Exit instead of continuing
            }
            total_sent += bytes_sent;
        }

        // Move file pointer to the beginning
        if (lseek(file_fd, 0, SEEK_SET) < 0) {
            perror("lseek failed");
            return;
        }

        // Send file contents
        char buffer[BUF_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = read(file_fd, buffer, BUF_SIZE)) > 0) {
            total_sent = 0;
            while (total_sent < bytes_read) {
                bytes_sent = send(clients[i], buffer + total_sent, bytes_read - total_sent, 0);
                if (bytes_sent < 0) {
                    perror("send file data");
                    return;  // Exit to prevent sending incomplete files
                }
                total_sent += bytes_sent;
            }
        }

        if (bytes_read < 0) {
            perror("read failed");
        }

        // Send end marker "&"
        // char end_marker = '&';
        // if (send(clients[i], &end_marker, 1, 0) < 0) {
        //     perror("send end marker");
        // }
    }

    pthread_mutex_unlock(&clients_mutex);
    close(file_fd);
}

void send_deletion_notice(const char *filepath){
    int len = strlen(filepath) - syncDir;
    char file_ka_naam[len + 2];  // +2 for '@' and '\0'

    char* iter = filepath;
    int i = syncDir;
    while (i--) iter++;
    
    i = 0;
    while (*iter) {
        file_ka_naam[i++] = *iter++;
    }
    
    file_ka_naam[len] = '@';  // Mark it as a delete command
    file_ka_naam[len + 1] = '\0';
    // printf("%s\n", file_ka_naam);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (send(clients[i], file_ka_naam, len + 2, 0) < 0) {
            perror("Failed to send delete notice");
        }
        // else printf("sent\n");
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Add a watch for a directory
void add_watch(const char *path) {
    if (watch_count >= MAX_WATCHES) {
        // fprintf(stderr, "Max watches reached\n");
        return;
    }
    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        perror("Couldn't add watch");
        return;
    }
    watch_list[watch_count].wd = wd;
    strncpy(watch_list[watch_count].path, path, PATH_MAX);
    watch_count++;
    printf("Watching: %s\n", path);
}

// Recursively watch directories
void add_watch_recursive(const char *base_path) {
    add_watch(base_path);
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    char path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            snprintf(path, PATH_MAX, "%s/%s/", base_path, entry->d_name);
            send_file_to_clients(path, '1');
            add_watch_recursive(path);
        } else if (entry->d_type == DT_REG) {
            snprintf(path, PATH_MAX, "%s/%s", base_path, entry->d_name);
            send_file_to_clients(path, '0');  // Send file metadata
        }
    }
    closedir(dir);
}

// Event handler
void handle_event(struct inotify_event *event) {
    for (int j = 0; j < watch_count; j++) {
        if (watch_list[j].wd == event->wd) {
            char full_path[PATH_MAX];
            snprintf(full_path, PATH_MAX, "%s/%s", watch_list[j].path, event->name);

            if (event->mask & IN_CREATE) {
                printf("%s was created\n", full_path);
                if (event->mask & IN_ISDIR) {
                    add_watch_recursive(full_path); // Add watch if it's a new directory
                    printf("%s\n",full_path);
                    send_file_to_clients(full_path, '1');
                }
                else {
                    send_file_to_clients(full_path, '0');
                }
            }
            else if (event->mask & IN_MOVED_TO) {
                printf("%s was moved in\n", full_path);
                if (event->mask & IN_ISDIR) {
                    add_watch_recursive(full_path); // Watch the new directory
                    send_file_to_clients(full_path, '1');
                }
                else{
                    send_file_to_clients(full_path, '0');
                }
            }
            else if (event->mask & IN_DELETE) {
                printf("%s was deleted\n", full_path);
                send_deletion_notice(full_path);
            }
            else if (event->mask & IN_MOVED_FROM) {
                printf("%s was moved out\n", full_path);
                send_deletion_notice(full_path);
            }
            break;
        }
    }
}

// Monitor directory for changes
void *watch_directory(void *arg) {
    const char *directory = (const char *)arg;
    fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init failed");
        exit(EXIT_FAILURE);
    }

    add_watch_recursive(directory);

    char buffer[BUFFER_SIZE];
    while (1) {
        int total_read = read(fd, buffer, BUFFER_SIZE);
        if (total_read < 0) {
            perror("read failed");
            exit(EXIT_FAILURE);
        }

        int i = 0;
        while (i < total_read) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            handle_event(event);
            i += MONITOR_EVENT_SIZE + event->len;
        }
    }
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./syncserver <directory> <port> <max_clients>\n");
        return 1;
    }

    syncDir = strlen(argv[1]);

    const char *directory = argv[1];
    int port = atoi(argv[2]);
    max_clients = atoi(argv[3]);

    // Start file monitoring in a separate thread
    pthread_t watcher_thread;
    pthread_create(&watcher_thread, NULL, watch_directory, (void *)directory);

    // Setup TCP server
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, max_clients);

    printf("Server started. Listening on port %d...\n", port);

    // Accept clients
    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }
        printf("Client connected\n");

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, (void *(*)(void *))handle_client, (void *)(intptr_t)client_sock);
        pthread_detach(client_thread);
    }

    close(server_sock);
    return 0;
}
