
# Real-Time File Synchronization System

This project implements a real-time, multi-client file synchronization system using low-level Linux system programming. It leverages inotify for monitoring directory changes and TCP sockets for network communication.

---

## 1. Files Included

### 1. `q1.c` – System Setup Script
This file is designed to **prepare the environment** before starting synchronization. It may:
- Create a base directory structure.
- Populate the initial files/folders to be monitored.
- Set appropriate permissions or clean up previous test runs.

> **Usage:**  
Compile and run to initialize the directory to be used by the server.
```bash
gcc q1.c -o setup && ./setup
```

---

### 2. `server.c` – File Monitoring and Broadcasting Server
This file sets up a **multi-threaded TCP server** that:
- Recursively watches a specified directory using `inotify`.
- Detects file/directory creation, deletion, and movement in real-time.
- Sends file changes to all connected clients (excluding filtered files).
- Uses threads to handle multiple clients concurrently.

> **Usage:**
```bash
gcc server.c -o syncserver -lpthread
./syncserver <directory_to_watch> <port> <max_clients>
```

Example:
```bash
./syncserver ./watched_dir 8080 5
```

---

### 3. `client.c` – File Receiver and Directory Sync Client
This file implements the **client-side application** that:
- Connects to the server via TCP.
- Sends an "ignore list" of file extensions to avoid syncing (e.g., `.mp4,.exe`).
- Listens for server updates and applies file or folder changes.
- Creates new directories or deletes files as instructed.

> **Usage:**
```bash
gcc client.c -o syncclient
./syncclient <server_ip> <port> <local_directory> <ignore_list_file>
```

Example:
```bash
./syncclient 127.0.0.1 8080 ./client_dir ignore.txt
```

---

## 2. Features

- Real-time synchronization using `inotify`.
- Multi-client support with threading.
- Per-client filtering of file types.
- Recursive directory monitoring and syncing.
- Lightweight and efficient design, suitable for virtual machines or offline use.

---

## 3. Requirements

- Linux system (with inotify support)
- GCC compiler
- Basic knowledge of terminal commands

---

## 4. How to Run the Project

1. **Compile and run `q1.c`** to set up the environment.
2. **Start the server** on the machine hosting the source directory:
   ```bash
   ./syncserver ./watched_dir 8080 5
   ```
3. **Start one or more clients** on the same or different machines:
   ```bash
   ./syncclient <server_ip> 8080 ./client_dir ignore.txt
   ```

---

## 5. Example `ignore.txt`
```
.mp4,.exe,.zip
```
> This will prevent the client from receiving any files with these extensions.

---

## 6. Note

- All file changes (create, move, delete) are synced from server to clients in real-time.
- Only one-way sync (server → clients) is currently implemented.
- Further improvements can include file update detection, bidirectional sync, and secure authentication.

