/* ─────────────────────────────────────────────────
   ST5004CEM — Task 4: Network Programming and IPC
   CLIENT

   Connects to the chat server, logs in with a
   username and password, then lets the user send
   messages and see messages from other clients at
   the same time. A separate thread listens for
   incoming messages so the client is not stuck
   waiting on user input before it can print them.
───────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 512

int sockfd;
int running = 1;

/* ─────────────────────────────────────────────────
   RECEIVER THREAD

   Constantly listens for messages from the server
   and prints them as they arrive, independent of
   whatever the user is typing.
───────────────────────────────────────────────── */
void *receive_loop(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];

    while (running) {
        int n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("\n[Client] Disconnected from server.\n");
            running = 0;
            break;
        }
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\r%s\n> ", buf);
        fflush(stdout);
    }
    return NULL;
}

/* send one line to the server, appending newline */
void send_line(const char *msg) {
    char out[BUF_SIZE];
    snprintf(out, sizeof(out), "%s\n", msg);
    if (send(sockfd, out, strlen(out), 0) < 0)
        perror("send failed");
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 8080;

    struct sockaddr_in server_addr;

    /* ── CREATE SOCKET ─────────────────────────── */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        exit(EXIT_FAILURE);
    }

    /* ── CONNECT ────────────────────────────────
       If the server is not running or unreachable,
       this fails cleanly instead of crashing.
    */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s:%d\n", server_ip, port);

    /* ── LOGIN OR REGISTER ─────────────────────── */
    char choice[8];
    printf("1. Login\n2. Register\nChoice: ");
    fgets(choice, sizeof(choice), stdin);

    char username[64], password[64], authmsg[BUF_SIZE], response[BUF_SIZE];

    if (choice[0] == '2') {
        /* REGISTRATION: send the request, print the result, then exit.
           The user runs the client again to log in with the new account. */
        printf("Choose a username: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0';

        printf("Choose a password (min 4 characters): ");
        fgets(password, sizeof(password), stdin);
        password[strcspn(password, "\n")] = '\0';

        snprintf(authmsg, sizeof(authmsg), "REGISTER %s %s", username, password);
        send_line(authmsg);

        int rn = recv(sockfd, response, sizeof(response) - 1, 0);
        if (rn <= 0) {
            printf("[Client] Server closed the connection.\n");
            close(sockfd);
            return 1;
        }
        response[rn] = '\0';
        response[strcspn(response, "\r\n")] = '\0';

        if (strncmp(response, "OK", 2) == 0)
            printf("[Client] Registered successfully. Run the client again to log in.\n");
        else
            printf("[Client] Registration failed: %s\n", response);

        close(sockfd);
        return 0;
    }

    /* LOGIN */
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';

    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0';

    snprintf(authmsg, sizeof(authmsg), "AUTH %s %s", username, password);
    send_line(authmsg);

    int n = recv(sockfd, response, sizeof(response) - 1, 0);
    if (n <= 0) {
        printf("[Client] Server closed the connection.\n");
        close(sockfd);
        return 1;
    }
    response[n] = '\0';
    response[strcspn(response, "\r\n")] = '\0';

    if (strncmp(response, "OK", 2) != 0) {
        printf("[Client] Login failed: %s\n", response);
        close(sockfd);
        return 1;
    }

    printf("[Client] Login successful. Type a message and press enter to send.\n");
    printf("[Client] Commands: LIST to see who is online, QUIT to disconnect.\n\n");

    /* ── START RECEIVER THREAD ─────────────────── */
    pthread_t tid;
    pthread_create(&tid, NULL, receive_loop, NULL);

    /* ── MAIN LOOP: read user input and send it ─── */
    char line[BUF_SIZE];
    while (running) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) continue;

        if (strcmp(line, "QUIT") == 0) {
            send_line("QUIT");
            running = 0;
            break;
        } else if (strcmp(line, "LIST") == 0) {
            send_line("LIST");
        } else {
            char msg[BUF_SIZE + 8];
            snprintf(msg, sizeof(msg), "MSG %s", line);
            send_line(msg);
        }
    }

    close(sockfd);
    return 0;
}