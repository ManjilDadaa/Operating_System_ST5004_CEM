# ST5004CEM — Operating Systems and Security

**Student:** Manjil Basnet
**Student ID:** 240623
**Module:** ST5004CEM — Operating Systems and Security
**College:** Softwarica College of IT & E-Commerce
**Language:** C

---

## Overview

This repository contains the coursework for the ST5004CEM Operating Systems and Security module. The assignment is divided into four tasks covering core OS concepts implemented in C: process management and threading, memory management simulation, file system operations and security, and network programming with IPC.

| Task | Topic | Status |
|------|-------|--------|
| Task 1 | Process Management and Threading | ✅ Completed |
| Task 2 | Memory Management Simulation | ✅ Completed |
| Task 3 | File System Operations and Security | ✅ Completed |
| Task 4 | Network Programming and IPC | ✅ Completed |

---

## Repository Structure

```
.
├── task1/          # Process management and threading
├── task2/          # Memory management simulation
├── task3/          # File system operations and security
├── task4/          # Network programming and IPC
├── docs/           # Reports, analysis, and screenshots
└── README.md
```

---

## Build Requirements

- GCC compiler (`gcc` 9.0 or above)
- Linux / Unix environment (tested on Ubuntu 22.04)
- POSIX threads library (`-lpthread`)

---

## Tasks

### Task 1 — Process Management and Threading

> Status: ✅ Completed

A round robin thread scheduler written with POSIX threads. Three threads take turns updating a shared counter under mutex protection, simulating fair CPU allocation with a fixed time quantum. Deadlock prevention is demonstrated using two additional resource locks acquired in a strict, consistent order across every thread, which removes the possibility of a circular wait.

**Concepts covered:** process and thread creation, mutexes and condition variables, round robin scheduling, race condition prevention, deadlock prevention through lock ordering.

**Build and run:**
```
cd task1
gcc -Wall -o main main.c -lpthread
./main
```

---

### Task 2 — Memory Management Simulation

> Status: ✅ Completed

A simulation of virtual memory and paging, comparing two page replacement algorithms, FIFO and LRU, against the same reference string. Each run tracks page hits and page faults per algorithm and prints a step by step trace of which page frame is loaded or evicted at every reference.

**Concepts covered:** virtual memory, paging and page tables, page faults, FIFO and LRU replacement algorithms, Belady's Anomaly.

**Build and run:**
```
cd task2
gcc -Wall -o main main.c
./main
```

---

### Task 3 — File System Operations and Security

> Status: ✅ Completed

A secure file management system with user accounts, Unix style file permissions, XOR based encryption and decryption, and a full audit log of every action taken. Users register and log in before any file operation is permitted, and every read, write, delete, and permission change is checked against ownership and access rules before being carried out.

**Concepts covered:** authentication and password hashing, Unix style permission bits (owner, group, others), sidecar metadata files, XOR encryption and decryption, audit logging.

**Build and run:**
```
cd task3
gcc -Wall -o main main.c
./main
```

---

### Task 4 — Network Programming and IPC

> Status: ✅ Completed

A multi client chat system built on TCP sockets, consisting of a server and a client program that communicate using a custom, line based text protocol. The server handles any number of clients concurrently, each on its own thread, with a mutex protected shared client list. Accounts can be registered and are authenticated the same way as Task 3, and every message is validated before being broadcast to the rest of the connected clients.

**Concepts covered:** socket programming (TCP), custom protocol design, concurrent client handling with threads, authentication and input validation, error handling and connection management.

**Build and run:**
```
cd task4
gcc -Wall -o server server.c -lpthread
gcc -Wall -o client client.c -lpthread
./server
```
In a separate terminal for each user:
```
./client
```

---

## Documentation

Full write ups for each task, including mathematical models, code explanations, output demonstrations, and security analysis, are available in the accompanying assignment report:

- Task 1: Round robin mathematical model, mutex usage, deadlock prevention, race condition handling
- Task 2: Paging simulation, FIFO vs LRU comparison, Belady's Anomaly discussion
- Task 3: Authentication mechanism, file permission system, encryption and decryption, audit logging, security analysis
- Task 4: Protocol documentation, concurrent client handling, security measures, testing documentation

---

## References

- POSIX Threads Programming, `pthread.h` documentation
- Beej's Guide to Network Programming, for socket programming reference
- Silberschatz, Galvin, and Gagne, *Operating System Concepts*, for scheduling, memory management, and file system theory