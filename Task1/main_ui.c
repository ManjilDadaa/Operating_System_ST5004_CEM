#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

/* ─────────────────────────────────────────────────
   CONSTANTS
───────────────────────────────────────────────── */
#define NUM_THREADS 3
#define TIME_SLICE  1    /* seconds each thread works per round */
#define TOTAL_WORK  3    /* rounds each thread must complete    */
#define BAR_WIDTH   20   /* character width of progress bars   */

/* ─────────────────────────────────────────────────
   ANSI ESCAPE CODES — colours and effects
   These make the terminal output colourful.
   \033[ is the escape sequence start.
───────────────────────────────────────────────── */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

/* CLEAR screen + move cursor to top-left */
#define CLEAR   "\033[2J\033[H"

/* One colour per thread so they're visually distinct */
const char *T_COLOR[3] = {CYAN, MAGENTA, YELLOW};
const char *T_NAME[3]  = {"Thread-0", "Thread-1", "Thread-2"};

/* ─────────────────────────────────────────────────
   SHARED DATA + MUTEX

   shared_counter is touched by all threads.
   Without counter_mutex, two threads could write
   at the same time — corrupting the value.
───────────────────────────────────────────────── */
int shared_counter = 0;
pthread_mutex_t counter_mutex;

/* ─────────────────────────────────────────────────
   ROUND-ROBIN SCHEDULER STATE

   current_turn holds the ID (0/1/2) of whichever
   thread is allowed to work right now.
   scheduler_mutex protects current_turn itself.
   turn_cond lets threads sleep until their turn.
───────────────────────────────────────────────── */
int current_turn = 0;
pthread_mutex_t scheduler_mutex;
pthread_cond_t  turn_cond;

/* ─────────────────────────────────────────────────
   DEADLOCK-PREVENTION RESOURCES

   Deadlock needs 4 conditions at once: mutual
   exclusion, hold-and-wait, no preemption, and
   circular wait. Two locked resources (resource_A,
   resource_B) create the setup where circular wait
   COULD happen — e.g. one thread grabs A then wants
   B, while another grabs B then wants A, and both
   wait forever.

   PREVENTION: every thread here always locks
   resource_A before resource_B, never the reverse.
   With a fixed global order, no thread can ever be
   holding B while waiting on A, so a circular chain
   of waits can never form. This breaks the circular
   wait condition, which is enough on its own to make
   deadlock impossible, regardless of scheduling.
───────────────────────────────────────────────── */
pthread_mutex_t resource_A;
pthread_mutex_t resource_B;

/* ─────────────────────────────────────────────────
   THREAD ARGUMENT + STATE STRUCT

   Each thread gets its own copy of this struct.
   The UI reads state[] and rounds_done to display
   what each thread is currently doing.
───────────────────────────────────────────────── */
typedef struct {
    int thread_id;
    int rounds_done;
    char state[16]; /* "WAITING"|"RUNNING"|"LOCKING"|"RES-A"|"RES-B"|"DONE" */
} ThreadArgs;

ThreadArgs targs[NUM_THREADS];

/* ─────────────────────────────────────────────────
   MUTEX DISPLAY STATE

   These variables track which mutexes are locked
   and who holds them — purely for the UI display.
   ui_mutex protects these display variables.
───────────────────────────────────────────────── */
int counter_mutex_locked = 0;
int counter_mutex_holder = -1;
int sched_mutex_locked   = 0;
int sched_mutex_holder   = -1;
int resA_locked = 0;
int resA_holder = -1;
int resB_locked = 0;
int resB_holder = -1;

pthread_mutex_t ui_mutex; /* prevents two threads drawing at once */

/* ─────────────────────────────────────────────────
   DRAW HELPERS
───────────────────────────────────────────────── */

/* Prints a filled/empty block progress bar */
void print_bar(int done, int total, const char *color) {
    int filled = (done * BAR_WIDTH) / total;
    printf("%s[", color);
    for (int i = 0; i < BAR_WIDTH; i++)
        printf(i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91"); /* █ or ░ */
    printf("]" RESET);
}

/* Small helper to print one mutex row in the panel */
void print_mutex_row(const char *label, int locked, int holder) {
    printf("    %-16s: ", label);
    if (locked)
        printf(RED "[LOCKED]" RESET "  held by %s%s\n" RESET,
               T_COLOR[holder], T_NAME[holder]);
    else
        printf(GREEN "[FREE]" RESET "\n");
}

/* ─────────────────────────────────────────────────
   DRAW UI

   Clears the terminal and redraws the full display.
   Called every time something meaningful changes.
   ui_mutex must be held by the caller.
───────────────────────────────────────────────── */
void draw_ui(void) {
    printf(CLEAR);

    /* Header */
    printf(BOLD BLUE
        "\n"
        "  \xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n"
        "  \xe2\x95\x91  ST5004CEM \xe2\x80\x94 Task 1: Round-Robin Thread Scheduler   \xe2\x95\x91\n"
        "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n"
        RESET "\n");

    /* Shared counter */
    printf(BOLD "  Shared Counter : " RESET GREEN BOLD "%d" RESET
           DIM "  (target: %d  =  %d threads x %d rounds)\n\n" RESET,
           shared_counter,
           NUM_THREADS * TOTAL_WORK, NUM_THREADS, TOTAL_WORK);

    /* Thread table header */
    printf(BOLD "  %-12s  %-8s  %-24s  %s\n" RESET,
           "Thread", "Rounds", "Progress", "State");
    printf(DIM "  ────────────  ────────  ────────────────────────  ─────────\n" RESET);

    /* One row per thread */
    for (int i = 0; i < NUM_THREADS; i++) {
        ThreadArgs *t = &targs[i];

        /* Pick a colour for the state label */
        const char *sc =
            strcmp(t->state, "DONE")    == 0 ? GREEN        :
            strcmp(t->state, "RUNNING") == 0 ? T_COLOR[i]   :
            strcmp(t->state, "LOCKING") == 0 ? YELLOW       :
            strcmp(t->state, "RES-A")   == 0 ? YELLOW       :
            strcmp(t->state, "RES-B")   == 0 ? YELLOW       : DIM;

        /* Thread name + round count */
        printf("  %s%-12s" RESET "  %s%d / %d%s  ",
               T_COLOR[i], T_NAME[i],
               BOLD, t->rounds_done, TOTAL_WORK, RESET);

        /* Progress bar */
        print_bar(t->rounds_done, TOTAL_WORK, T_COLOR[i]);

        /* State label */
        printf("  %s%-9s" RESET "\n", sc, t->state);
    }

    /* Scheduler current turn */
    printf("\n" BOLD "  Scheduler turn  : " RESET);
    printf("%s%s\n" RESET, T_COLOR[current_turn], T_NAME[current_turn]);

    /* Mutex status panel */
    printf("\n" BOLD "  Mutex locks:\n" RESET);
    print_mutex_row("counter_mutex",   counter_mutex_locked, counter_mutex_holder);
    print_mutex_row("scheduler_mutex", sched_mutex_locked,   sched_mutex_holder);
    print_mutex_row("resource_A",      resA_locked,          resA_holder);
    print_mutex_row("resource_B",      resB_locked,          resB_holder);

    /* Deadlock note */
    printf("\n" DIM
           "  Deadlock prevention: resource_A is always locked before\n"
           "  resource_B, by every thread, with no exceptions. This fixed\n"
           "  ordering makes a circular wait impossible, so deadlock\n"
           "  cannot occur no matter how threads are scheduled.\n" RESET);
}

/* ─────────────────────────────────────────────────
   THREAD FUNCTION

   Every thread runs this same function.
   Each loop iteration = one round of work:
     1. Wait for our turn        (round-robin)
     2. Simulate work            (sleep TIME_SLICE)
     3. Lock counter_mutex       → increment → unlock
     4. Lock resource_A then resource_B (ordered)
        → use both → unlock B then A (deadlock prevention)
     5. Pass turn to next thread
───────────────────────────────────────────────── */
void *thread_task(void *arg) {
    ThreadArgs *t = (ThreadArgs *)arg;
    int id = t->thread_id;

    for (int round = 0; round < TOTAL_WORK; round++) {

        /* ── Step 1: wait for our turn ─────────────── */
        pthread_mutex_lock(&ui_mutex);
        strcpy(t->state, "WAITING");
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_lock(&scheduler_mutex);

        /* Update display: scheduler_mutex now locked */
        pthread_mutex_lock(&ui_mutex);
        sched_mutex_locked = 1;
        sched_mutex_holder = id;
        pthread_mutex_unlock(&ui_mutex);

        /* Sleep until it is this thread's turn */
        while (current_turn != id)
            pthread_cond_wait(&turn_cond, &scheduler_mutex);

        /* Update display: scheduler_mutex about to be released */
        pthread_mutex_lock(&ui_mutex);
        sched_mutex_locked = 0;
        sched_mutex_holder = -1;
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_unlock(&scheduler_mutex);

        /* ── Step 2: simulate work ──────────────────── */
        pthread_mutex_lock(&ui_mutex);
        strcpy(t->state, "RUNNING");
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        sleep(TIME_SLICE); /* simulate TIME_SLICE seconds of CPU work */

        /* ── Step 3: update shared counter safely ───── */

        /* Show that we are about to lock counter_mutex */
        pthread_mutex_lock(&ui_mutex);
        strcpy(t->state, "LOCKING");
        counter_mutex_locked = 1;
        counter_mutex_holder = id;
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        /* CRITICAL SECTION — only one thread here at a time */
        pthread_mutex_lock(&counter_mutex);
        shared_counter++;
        t->rounds_done++;
        pthread_mutex_unlock(&counter_mutex);
        /* END CRITICAL SECTION */

        /* counter_mutex is now free */
        pthread_mutex_lock(&ui_mutex);
        counter_mutex_locked = 0;
        counter_mutex_holder = -1;
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        /* ── Step 4: deadlock-safe two-resource access ─
         * Every thread locks resource_A FIRST, then
         * resource_B — always this order, never reversed.
         * That fixed order is what prevents circular wait
         * between threads, and therefore prevents deadlock.
         */
        pthread_mutex_lock(&ui_mutex);
        strcpy(t->state, "RES-A");
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_lock(&resource_A);
        pthread_mutex_lock(&ui_mutex);
        resA_locked = 1;
        resA_holder = id;
        strcpy(t->state, "RES-B");
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_lock(&resource_B);
        pthread_mutex_lock(&ui_mutex);
        resB_locked = 1;
        resB_holder = id;
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        /* ... pretend to do work needing both resources ... */

        pthread_mutex_unlock(&resource_B);
        pthread_mutex_lock(&ui_mutex);
        resB_locked = 0;
        resB_holder = -1;
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_unlock(&resource_A);
        pthread_mutex_lock(&ui_mutex);
        resA_locked = 0;
        resA_holder = -1;
        draw_ui();
        pthread_mutex_unlock(&ui_mutex);

        /* ── Step 5: pass turn to next thread ──────── */
        pthread_mutex_lock(&scheduler_mutex);

        pthread_mutex_lock(&ui_mutex);
        sched_mutex_locked = 1;
        sched_mutex_holder = id;
        pthread_mutex_unlock(&ui_mutex);

        /*
         * Round-robin: (0+1)%3=1, (1+1)%3=2, (2+1)%3=0
         * The modulo wraps it back to 0 after Thread-2.
         */
        current_turn = (current_turn + 1) % NUM_THREADS;

        /*
         * Wake ALL sleeping threads so each one can
         * check whether it is now their turn.
         * (Using broadcast instead of signal avoids
         *  the risk of waking the wrong thread.)
         */
        pthread_cond_broadcast(&turn_cond);

        pthread_mutex_lock(&ui_mutex);
        sched_mutex_locked = 0;
        sched_mutex_holder = -1;
        pthread_mutex_unlock(&ui_mutex);

        pthread_mutex_unlock(&scheduler_mutex);
    }

    /* All rounds done */
    pthread_mutex_lock(&ui_mutex);
    strcpy(t->state, "DONE");
    draw_ui();
    pthread_mutex_unlock(&ui_mutex);

    return NULL;
}

/* ─────────────────────────────────────────────────
   MAIN
───────────────────────────────────────────────── */
int main(void) {
    pthread_t threads[NUM_THREADS];

    /* Initialise all mutexes and the condition variable */
    pthread_mutex_init(&counter_mutex,   NULL);
    pthread_mutex_init(&scheduler_mutex, NULL);
    pthread_mutex_init(&ui_mutex,        NULL);
    pthread_mutex_init(&resource_A,      NULL);
    pthread_mutex_init(&resource_B,      NULL);
    pthread_cond_init(&turn_cond,        NULL);

    /* Set up per-thread state */
    for (int i = 0; i < NUM_THREADS; i++) {
        targs[i].thread_id   = i;
        targs[i].rounds_done = 0;
        strcpy(targs[i].state, "WAITING");
    }

    /* Draw the initial empty UI before threads start */
    draw_ui();

    /* Spawn all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_task, &targs[i]) != 0) {
            fprintf(stderr, "Error: could not create thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    /* Final result */
    pthread_mutex_lock(&ui_mutex);
    draw_ui();
    printf("\n" BOLD GREEN
           "  All threads completed. No deadlock occurred.\n"
           "  Final shared counter = %d  (expected %d)\n\n" RESET,
           shared_counter, NUM_THREADS * TOTAL_WORK);
    pthread_mutex_unlock(&ui_mutex);

    /* Clean up */
    pthread_mutex_destroy(&counter_mutex);
    pthread_mutex_destroy(&scheduler_mutex);
    pthread_mutex_destroy(&ui_mutex);
    pthread_mutex_destroy(&resource_A);
    pthread_mutex_destroy(&resource_B);
    pthread_cond_destroy(&turn_cond);

    return 0;
}