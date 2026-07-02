#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ───────────────────────────────────────────────────────────────────────────────────
   CONFIGURATION — yo value haru change garera different scenario ma test garna milxa.
────────────────────────────────────────────────────────────────────────────────────── */
#define PAGE_SIZE        4     // bytes per page
#define FRAME_COUNT      4     // physical frames in RAM
#define REF_LENGTH       12    // length of the reference string

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
#define WHITE   "\033[37m"
