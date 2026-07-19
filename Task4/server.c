/* ─────────────────────────────────────────────────
   ST5004CEM — Task 4: Network Programming and IPC
   SERVER

   A simple TCP chat server. Each client connects,
   authenticates with a username and password, and
   can then send messages that get broadcast to every
   other connected client. This demonstrates sockets,
   a custom text protocol, multiple concurrent clients
   (one thread per client), basic authentication and
   input validation, and error handling throughout.
───────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT           8080
#define MAX_CLIENTS    10
#define BUF_SIZE       512
#define MAX_USERNAME   32
#define MAX_PASSHASH   128
#define MAX_MSG        400
#define MAX_ACCOUNTS   50
#define ACCOUNTS_FILE  "chat_users.db"
#define XOR_KEY        0x5A

/* ─────────────────────────────────────────────────
   ACCOUNT STORE
───────────────────────────────────────────────── */
typedef struct {
    char username[MAX_USERNAME];
    char password_hash[MAX_PASSHASH]; /* hex string of XOR(password, XOR_KEY) */
} Account;

Account accounts[MAX_ACCOUNTS];
int accounts_count = 0;
pthread_mutex_t accounts_mutex;

void hash_password(const char *pw, char *out) {
    int len = strlen(pw);
    for (int i = 0; i < len; i++)
        sprintf(out + i * 2, "%02x", (unsigned char)(pw[i] ^ XOR_KEY));
    out[len * 2] = '\0';
}

/* load saved accounts from disk into memory at startup */
void load_accounts(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "r");
    if (!f) return; /* first run: no file yet, start with zero accounts */

    while (accounts_count < MAX_ACCOUNTS &&
           fscanf(f, "%31s %127s",
                  accounts[accounts_count].username,
                  accounts[accounts_count].password_hash) == 2) {
        accounts_count++;
    }
    fclose(f);
}

/* rewrite the whole accounts file from what is currently in memory.
   Caller must already hold accounts_mutex. */
void save_accounts_locked(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "w");
    if (!f) return;
    for (int i = 0; i < accounts_count; i++)
        fprintf(f, "%s %s\n", accounts[i].username, accounts[i].password_hash);
    fclose(f);
}

/* returns 1 if username/password match a known account */
int authenticate(const char *username, const char *password) {
    char hashed[MAX_PASSHASH];
    hash_password(password, hashed);

    pthread_mutex_lock(&accounts_mutex);
    int ok = 0;
    for (int i = 0; i < accounts_count; i++) {
        if (strcmp(accounts[i].username, username) == 0 &&
            strcmp(accounts[i].password_hash, hashed) == 0) {
            ok = 1;
            break;
        }
    }
    pthread_mutex_unlock(&accounts_mutex);
    return ok;
}

/* checks basic rules: 3-31 chars, only letters/digits/underscore */
int is_valid_username(const char *username) {
    int len = strlen(username);
    if (len < 3 || len >= MAX_USERNAME) return 0;
    for (int i = 0; i < len; i++) {
        char c = username[i];
        if (!isalnum((unsigned char)c) && c != '_') return 0;
    }
    return 1;
}

/* returns: 1 = registered, 0 = username taken, -1 = invalid input */
int register_account(const char *username, const char *password) {
    if (!is_valid_username(username)) return -1;
    if (strlen(password) < 4) return -1;

    pthread_mutex_lock(&accounts_mutex);

    for (int i = 0; i < accounts_count; i++) {
        if (strcmp(accounts[i].username, username) == 0) {
            pthread_mutex_unlock(&accounts_mutex);
            return 0; /* username already taken */
        }
    }

    if (accounts_count >= MAX_ACCOUNTS) {
        pthread_mutex_unlock(&accounts_mutex);
        return -1;
    }

    strncpy(accounts[accounts_count].username, username, MAX_USERNAME - 1);
    hash_password(password, accounts[accounts_count].password_hash);
    accounts_count++;
    save_accounts_locked();

    pthread_mutex_unlock(&accounts_mutex);
    return 1;
}

/* ─────────────────────────────────────────────────
   CONNECTED CLIENT LIST

   Shared across all threads, so it is protected by
   clients_mutex. This is the same idea as the
   shared_counter mutex from Task 1: any thread that
   touches the client list must lock first.
───────────────────────────────────────────────── */
typedef struct {
    int  sockfd;
    char username[MAX_USERNAME];
    int  in_use;
} ClientSlot;

ClientSlot clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex;

void clients_init(void) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].in_use = 0;
}

/* send one line to a single socket, appending newline */
void send_line(int sockfd, const char *msg) {
    char out[BUF_SIZE];
    snprintf(out, sizeof(out), "%s\n", msg);
    send(sockfd, out, strlen(out), 0); /* return value checked by caller if needed */
}

/* send a line to every authenticated client except 'exclude_fd' */
void broadcast(const char *msg, int exclude_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].sockfd != exclude_fd)
            send_line(clients[i].sockfd, msg);
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* remove a client from the shared list once it disconnects */
void remove_client(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].sockfd == sockfd) {
            clients[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* ─────────────────────────────────────────────────
   PER-CLIENT THREAD

   One of these runs for every connected client, so
   the server can talk to many clients at once. This
   is the IPC side of the task: the socket is the
   channel each client process/thread uses to
   exchange data with the server.
───────────────────────────────────────────────── */
void *handle_client(void *arg) {
    int sockfd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    char username[MAX_USERNAME] = "";

    // ── STEP 1: LOGIN OR REGISTER ───────────────
    
    int n = recv(sockfd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { close(sockfd); return NULL; }
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char cmd[16], uname[MAX_USERNAME], pass[MAX_USERNAME];
    int parsed = sscanf(buf, "%15s %31s %31s", cmd, uname, pass) == 3;

    if (parsed && strcmp(cmd, "REGISTER") == 0) {
        int result = register_account(uname, pass);
        if (result == 1) {
            send_line(sockfd, "OK REGISTER_SUCCESS");
        } else if (result == 0) {
            send_line(sockfd, "ERR USERNAME_TAKEN");
        } else {
            send_line(sockfd, "ERR INVALID_INPUT");
        }
        /* registering does not log the client in automatically;
           they reconnect and log in like anyone else */
        close(sockfd);
        return NULL;
    }

    if (parsed && strcmp(cmd, "AUTH") == 0 && authenticate(uname, pass)) {
        strncpy(username, uname, MAX_USERNAME - 1);
        send_line(sockfd, "OK AUTH_SUCCESS");
    } else {
        send_line(sockfd, "ERR AUTH_FAILED");
        close(sockfd);
        return NULL;
    }

    /* Register this client in the shared list */
    pthread_mutex_lock(&clients_mutex);
    int registered = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].in_use) {
            clients[i].in_use = 1;
            clients[i].sockfd = sockfd;
            strncpy(clients[i].username, username, MAX_USERNAME - 1);
            registered = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!registered) {
        send_line(sockfd, "ERR SERVER_FULL");
        close(sockfd);
        return NULL;
    }

    printf("[Server] %s connected.\n", username);
    char notice[BUF_SIZE];
    snprintf(notice, sizeof(notice), "INFO %s has joined the chat", username);
    broadcast(notice, sockfd);

    /* ── STEP 2: MAIN PROTOCOL LOOP ──────────────
       After login, the client can send:
         MSG <text>   -> broadcast to everyone else
         LIST         -> list connected usernames
         QUIT         -> disconnect cleanly
       Anything else is rejected as invalid input.
    */
    while (1) {
        n = recv(sockfd, buf, sizeof(buf) - 1, 0);

        /* recv() returning 0 or a negative value means the
           client disconnected or the connection broke */
        if (n <= 0) {
            printf("[Server] %s disconnected.\n", username);
            break;
        }
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';

        if (strncmp(buf, "MSG ", 4) == 0) {
            char *text = buf + 4;

            /* DATA VALIDATION: reject empty or oversized messages */
            if (strlen(text) == 0) {
                send_line(sockfd, "ERR EMPTY_MESSAGE");
                continue;
            }
            if (strlen(text) > MAX_MSG) {
                send_line(sockfd, "ERR MESSAGE_TOO_LONG");
                continue;
            }

            char out[BUF_SIZE];
            snprintf(out, sizeof(out), "MSG %s: %s", username, text);
            broadcast(out, sockfd);
            send_line(sockfd, "OK SENT");
        }
        else if (strcmp(buf, "LIST") == 0) {
            char out[BUF_SIZE] = "OK ONLINE:";
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].in_use) {
                    strcat(out, " ");
                    strcat(out, clients[i].username);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            send_line(sockfd, out);
        }
        else if (strcmp(buf, "QUIT") == 0) {
            send_line(sockfd, "OK BYE");
            break;
        }
        else {
            /* DATA VALIDATION: unknown command is rejected,
               not silently ignored or crashed on */
            send_line(sockfd, "ERR UNKNOWN_COMMAND");
        }
    }

    /* ── STEP 3: CLEANUP ─────────────────────────
       Always remove the client and tell everyone
       else they left, even if the disconnect was
       unexpected (e.g. network drop).
    */
    remove_client(sockfd);
    snprintf(notice, sizeof(notice), "INFO %s has left the chat", username);
    broadcast(notice, sockfd);
    close(sockfd);
    return NULL;
}

int main(void) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    clients_init();
    pthread_mutex_init(&clients_mutex, NULL);
    pthread_mutex_init(&accounts_mutex, NULL);
    load_accounts();
    printf("[Server] Loaded %d existing account(s).\n", accounts_count);

    /* ── CREATE SOCKET ───────────────────────────
       AF_INET = IPv4, SOCK_STREAM = TCP (reliable,
       connection-based — needed for a chat protocol).
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    /* Allow the port to be reused immediately after restart,
       instead of waiting for the OS to release it. */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    /* ── BIND ─────────────────────────────────── */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* ── LISTEN ───────────────────────────────────
       Backlog of MAX_CLIENTS pending connections.
    */
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d...\n", PORT);

    /* ── ACCEPT LOOP ──────────────────────────────
       Every new connection gets its own thread, so
       multiple clients can be handled at the same
       time without blocking each other.
    */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept failed");
            continue; /* one bad accept should not kill the server */
        }

        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, fd_ptr) != 0) {
            perror("pthread_create failed");
            close(client_fd);
            free(fd_ptr);
            continue;
        }
        pthread_detach(tid); /* thread cleans up its own resources when done */
    }

    close(server_fd);
    pthread_mutex_destroy(&clients_mutex);
    pthread_mutex_destroy(&accounts_mutex);
    return 0;
}