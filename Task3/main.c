#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ─────────────────────────────────────────────────
   ANSI COLOURS
───────────────────────────────────────────────── */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define CLEAR   "\033[2J\033[H"

/* ─────────────────────────────────────────────────
   CONSTANTS
───────────────────────────────────────────────── */
#define MAX_USERS    5
#define MAX_NAME     32
#define MAX_PASS     64
#define MAX_INPUT    512
#define AUDIT_LOG    "audit.log"
#define USER_DB      "users.db"
#define FILES_DIR    "fs_files"
#define XOR_KEY      0x5A

/* ─────────────────────────────────────────────────
   PERMISSION BITS  (Unix-style octal)
───────────────────────────────────────────────── */
#define PERM_OWNER_R  0400
#define PERM_OWNER_W  0200
#define PERM_OWNER_X  0100
#define PERM_OTHER_R  0004
#define PERM_OTHER_W  0002

/* ─────────────────────────────────────────────────
   STRUCTS
───────────────────────────────────────────────── */
typedef struct {
    char username[MAX_NAME];
    char password_hash[MAX_PASS];
    int  is_owner;
} User;

/* ─────────────────────────────────────────────────
   SESSION
───────────────────────────────────────────────── */
User current_user;
int  logged_in = 0;

/* ─────────────────────────────────────────────────
   HELPERS
───────────────────────────────────────────────── */

/* Read a line from stdin, strip newline */
void input(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, size, stdin))
        buf[strcspn(buf, "\n")] = '\0';
}

/* Read password without echo */
void input_password(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    system("stty -echo");
    if (fgets(buf, size, stdin))
        buf[strcspn(buf, "\n")] = '\0';
    system("stty echo");
    printf("\n");
}

void divider(void) {
    printf("============================================================\n");
}

void header(const char *title) {
    printf("\n");
    divider();
    printf("  %s\n", title);
    divider();
    printf("\n");
}

/* Permission guide — shown wherever user picks permissions */
void print_perm_guide(void) {
    printf("  Permission Guide:\n");
    printf("    600  =  only you can read and write             (rw-------)\n");
    printf("    640  =  you can read/write, group can read      (rw-r-----)\n");
    printf("    644  =  you can read/write, everyone can read   (rw-r--r--)\n");
    printf("    700  =  only you can read, write and execute    (rwx------)\n");
    printf("    777  =  everyone can read, write and execute    (rwxrwxrwx)\n");
    printf("\n");
}

/* ─────────────────────────────────────────────────
   PASSWORD HASH
   XOR each byte with XOR_KEY, store as hex string.
   Passwords are never saved in plaintext.
───────────────────────────────────────────────── */
void hash_password(const char *pw, char *out) {
    int len = strlen(pw);
    for (int i = 0; i < len; i++)
        sprintf(out + i * 2, "%02x", (unsigned char)(pw[i] ^ XOR_KEY));
    out[len * 2] = '\0';
}

/* ─────────────────────────────────────────────────
   AUDIT LOG
   Every action is recorded with timestamp, user,
   action type, and filename.
───────────────────────────────────────────────── */
void audit(const char *action, const char *file) {
    FILE *f = fopen(AUDIT_LOG, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] USER=%-14s ACTION=%-14s FILE=%s\n",
            ts,
            logged_in ? current_user.username : "anonymous",
            action,
            file ? file : "-");
    fclose(f);
}

/* ─────────────────────────────────────────────────
   USER DB HELPERS
───────────────────────────────────────────────── */
int load_users(User *users, int *count) {
    FILE *f = fopen(USER_DB, "r");
    if (!f) { *count = 0; return 0; }
    *count = 0;
    while (*count < MAX_USERS &&
           fscanf(f, "%31s %63s %d",
                  users[*count].username,
                  users[*count].password_hash,
                  &users[*count].is_owner) == 3)
        (*count)++;
    fclose(f);
    return 1;
}

void save_users(User *users, int count) {
    FILE *f = fopen(USER_DB, "w");
    if (!f) return;
    for (int i = 0; i < count; i++)
        fprintf(f, "%s %s %d\n",
                users[i].username,
                users[i].password_hash,
                users[i].is_owner);
    fclose(f);
}
