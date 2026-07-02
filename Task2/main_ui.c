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

/* ─────────────────────────────────────────────────
   PAGE FRAME — one slot in physical memory
───────────────────────────────────────────────── */
typedef struct {
    int  page_id;    /* which page is loaded (-1 = empty) */
    int  loaded_at;  /* tick when page was loaded (for FIFO) */
    int  last_used;  /* tick of last access        (for LRU)  */
    char data[PAGE_SIZE]; /* simulated page content            */
} Frame;

/* ─────────────────────────────────────────────────
   STATS — tracked per algorithm run
───────────────────────────────────────────────── */
typedef struct {
    int hits;
    int faults;
} Stats;


/* Returns index of page_id in frames[], or -1 if not present */
int find_page(Frame *frames, int page_id) {
    for (int i = 0; i < FRAME_COUNT; i++)
        if (frames[i].page_id == page_id) return i;
    return -1;
}

/* Returns 1 if any frame is empty */
int has_empty(Frame *frames) {
    for (int i = 0; i < FRAME_COUNT; i++)
        if (frames[i].page_id == -1) return 1;
    return 0;
}

/* Returns index of first empty frame */
int first_empty(Frame *frames) {
    for (int i = 0; i < FRAME_COUNT; i++)
        if (frames[i].page_id == -1) return i;
    return -1;
}

/* ─────────────────────────────────────────────────
   PRINT FRAME STATE — the visual buffer display
   Shows all frames at this point in time
───────────────────────────────────────────────── */
void print_frames(Frame *frames, int ref, int tick, int is_fault) {
    printf("  Ref %-3d │ ", ref);
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frames[i].page_id == -1)
            printf(DIM " [ — ]" RESET);
        else if (frames[i].page_id == ref)
            printf(CYAN " [P%-2d]" RESET, frames[i].page_id);
        else
            printf(GREEN " [P%-2d]" RESET, frames[i].page_id);
    }
    if (is_fault)
        printf("  " RED "PAGE FAULT" RESET "\n");
    else
        printf("  " GREEN "HIT" RESET "\n");
}

/* ─────────────────────────────────────────────────
   SIMULATE LOADING — fill a page with dummy data
   (represents the OS copying a page from disk)
───────────────────────────────────────────────── */
void load_page(Frame *f, int page_id, int tick) {
    f->page_id   = page_id;
    f->loaded_at = tick;
    f->last_used = tick;
    /* fill with identifiable dummy bytes */
    for (int b = 0; b < PAGE_SIZE; b++)
        f->data[b] = (char)('A' + page_id);
    printf(YELLOW "    → Loading P%d into frame (data: " RESET, page_id);
    for (int b = 0; b < PAGE_SIZE; b++)
        printf("%c ", f->data[b]);
    printf(YELLOW ")\n" RESET);
}

/* ─────────────────────────────────────────────────
   PRINT HEADER
───────────────────────────────────────────────── */
void print_header(const char *algo_name, int *ref_string) {
    printf(BOLD BLUE
        "\n╔══════════════════════════════════════════════════════╗\n"
        "║  ST5004CEM Task 2 — Memory Management Simulation    ║\n"
        "╚══════════════════════════════════════════════════════╝\n"
        RESET);
    printf(BOLD "\n  Algorithm     : " RESET "%s\n", algo_name);
    printf(BOLD "  Page size     : " RESET "%d bytes\n", PAGE_SIZE);
    printf(BOLD "  Frame count   : " RESET "%d\n", FRAME_COUNT);
    printf(BOLD "  Reference str : " RESET);
    for (int i = 0; i < REF_LENGTH; i++)
        printf("P%d ", ref_string[i]);
    printf("\n\n");
    printf(BOLD "  Ref     │ Frames%-*s│ Result\n" RESET,
           FRAME_COUNT * 6 - 6, "");
    printf(DIM "  ────────┼");
    for (int i = 0; i < FRAME_COUNT * 6; i++) printf("─");
    printf("┼──────────\n" RESET);
}

/* ─────────────────────────────────────────────────
   FIFO PAGE REPLACEMENT
   Evicts whichever page was loaded FIRST (oldest).
───────────────────────────────────────────────── */
Stats run_fifo(int *ref_string) {
    Frame frames[FRAME_COUNT];
    for (int i = 0; i < FRAME_COUNT; i++) {
        frames[i].page_id   = -1;
        frames[i].loaded_at =  0;
        frames[i].last_used =  0;
        memset(frames[i].data, 0, PAGE_SIZE);
    }

    Stats s = {0, 0};
    int tick = 0;

    print_header("FIFO (First-In First-Out)", ref_string);

    for (int i = 0; i < REF_LENGTH; i++) {
        int ref = ref_string[i];
        tick++;

        int idx = find_page(frames, ref);

        if (idx != -1) {
            /* HIT — page is already in a frame */
            frames[idx].last_used = tick;
            s.hits++;
            print_frames(frames, ref, tick, 0);
        } else {
            /* PAGE FAULT */
            s.faults++;

            if (has_empty(frames)) {
                /* Empty frame available — just load */
                int slot = first_empty(frames);
                load_page(&frames[slot], ref, tick);
            } else {
                /* All frames full — evict the OLDEST (smallest loaded_at) */
                int oldest = 0;
                for (int j = 1; j < FRAME_COUNT; j++)
                    if (frames[j].loaded_at < frames[oldest].loaded_at)
                        oldest = j;
                printf(RED "    ✕ Evicting P%d (loaded earliest at tick %d)\n" RESET,
                       frames[oldest].page_id, frames[oldest].loaded_at);
                load_page(&frames[oldest], ref, tick);
            }
            print_frames(frames, ref, tick, 1);
        }
    }
    return s;
}

/* ─────────────────────────────────────────────────
   LRU PAGE REPLACEMENT
   Evicts whichever page was used LEAST RECENTLY.
───────────────────────────────────────────────── */
Stats run_lru(int *ref_string) {
    Frame frames[FRAME_COUNT];
    for (int i = 0; i < FRAME_COUNT; i++) {
        frames[i].page_id   = -1;
        frames[i].loaded_at =  0;
        frames[i].last_used =  0;
        memset(frames[i].data, 0, PAGE_SIZE);
    }

    Stats s = {0, 0};
    int tick = 0;

    print_header("LRU (Least Recently Used)", ref_string);

    for (int i = 0; i < REF_LENGTH; i++) {
        int ref = ref_string[i];
        tick++;

        int idx = find_page(frames, ref);

        if (idx != -1) {
            /* HIT — update last_used so LRU order stays correct */
            frames[idx].last_used = tick;
            s.hits++;
            print_frames(frames, ref, tick, 0);
        } else {
            /* PAGE FAULT */
            s.faults++;

            if (has_empty(frames)) {
                int slot = first_empty(frames);
                load_page(&frames[slot], ref, tick);
            } else {
                /* Evict the page with the SMALLEST last_used value */
                int lru = 0;
                for (int j = 1; j < FRAME_COUNT; j++)
                    if (frames[j].last_used < frames[lru].last_used)
                        lru = j;
                printf(YELLOW "    ✕ Evicting P%d (last used at tick %d)\n" RESET,
                       frames[lru].page_id, frames[lru].last_used);
                load_page(&frames[lru], ref, tick);
            }
            print_frames(frames, ref, tick, 1);
        }
    }
    return s;
}

/* ─────────────────────────────────────────────────
   PRINT STATS + COMPARISON
───────────────────────────────────────────────── */
void print_stats(const char *name, Stats s) {
    float hit_ratio   = (float)s.hits   / REF_LENGTH * 100.0f;
    float fault_ratio = (float)s.faults / REF_LENGTH * 100.0f;

    printf(BOLD "\n  ── %s Results ──\n" RESET, name);
    printf("  References  : %d\n", REF_LENGTH);
    printf("  Hits        : " GREEN "%d  (%.1f%%)\n" RESET, s.hits,   hit_ratio);
    printf("  Page Faults : " RED   "%d  (%.1f%%)\n" RESET, s.faults, fault_ratio);
}

void print_comparison(Stats fifo, Stats lru) {
    printf(BOLD BLUE
        "\n╔══════════════════════════════════════════════════════╗\n"
        "║              Algorithm Comparison                   ║\n"
        "╚══════════════════════════════════════════════════════╝\n"
        RESET);
    printf(BOLD "\n  %-20s %-10s %-10s %-10s\n" RESET,
           "Algorithm", "Hits", "Faults", "Hit Rate");
    printf(DIM "  ────────────────────  ──────────  ──────────  ──────────\n" RESET);

    float fifo_hr = (float)fifo.hits / REF_LENGTH * 100.0f;
    float lru_hr  = (float)lru.hits  / REF_LENGTH * 100.0f;

    printf("  %-20s " GREEN "%-10d" RESET " " RED "%-10d" RESET " %.1f%%\n",
           "FIFO", fifo.hits, fifo.faults, fifo_hr);
    printf("  %-20s " GREEN "%-10d" RESET " " RED "%-10d" RESET " %.1f%%\n",
           "LRU",  lru.hits,  lru.faults,  lru_hr);

    printf("\n  Verdict: ");
    if (lru.faults < fifo.faults)
        printf(GREEN "LRU produced fewer page faults (%d vs %d) on this reference string.\n" RESET,
               lru.faults, fifo.faults);
    else if (fifo.faults < lru.faults)
        printf(YELLOW "FIFO produced fewer page faults (%d vs %d) on this reference string.\n" RESET,
               fifo.faults, lru.faults);
    else
        printf(CYAN "Both algorithms produced the same number of page faults.\n" RESET);

    printf(DIM "\n  Note: LRU is generally superior but requires tracking access\n"
               "  history. FIFO is simpler but can suffer Belady's Anomaly —\n"
               "  adding more frames can sometimes increase page faults.\n" RESET);
}

/* ─────────────────────────────────────────────────
   MAIN
───────────────────────────────────────────────── */
int main(void) {
    /*
     * Reference string: the sequence of page numbers
     * the CPU requests. This simulates a program
     * accessing pages in this order.
     *
     * Pages 0–4 represent 5 distinct virtual pages.
     * With only 4 frames, the simulator must decide
     * which page to evict when all frames are full.
     */
    int ref_string[REF_LENGTH] = {0, 1, 2, 3, 0, 1, 4, 0, 1, 2, 3, 4};

    Stats fifo_stats = run_fifo(ref_string);
    print_stats("FIFO", fifo_stats);

    printf("\n\n");

    Stats lru_stats = run_lru(ref_string);
    print_stats("LRU", lru_stats);

    print_comparison(fifo_stats, lru_stats);

    printf("\n");
    return 0;
}