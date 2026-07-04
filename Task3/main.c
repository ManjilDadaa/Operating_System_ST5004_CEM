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