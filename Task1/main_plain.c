#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<string.h>

// Constants

#define NUM_THREADS 3
#define TIME_SLICE 1
#define TOTAL_WORK 3

int shared_counter = 0; // sabai thread le access garna milne variable, jasko value sabai thread le change garna sakxa.
pthread_mutex_t counter_mutex; // aru thread le shared_counter variable lai change garna namilos vanera mutex use gareko



// Round Robin Scheduler State
/* 
   ROUND-ROBIN SCHEDULER STATE
   
   The scheduler gives each thread one turn at
   a time in order: 0 → 1 → 2 → 0 → 1 → 2 ...
   'current_turn' tracks whose turn it is.
   'scheduler_mutex' and 'turn_cond' let threads
   wait until it is their turn.

*/
int current_turn = 0; // Kun tread ko palo ho vanxa, jun value xa, tei thread ko turn 
pthread_mutex_t scheduler_mutex; // aru thread haru le current_turn variable lai change garna namilos vanera mutex use gareko
pthread_cond_t turn_cond; // aru thread le afno turn aayo ki aayena vanera check gari rakhnu vanda 
                          // sleep mode ma janxa ani euta thread ko kaam sakey paxi sabai le check garxa, 
                          // jasko palo ho tyo kaam garxa aru back to sleep.


typedef struct {
    int thread_id; 
} ThreadArgs;

/* 
   THREAD FUNCTION
   
   Every thread runs this same function.
   It loops TOTAL_WORK times. Each loop:
     1. Waits for its turn (round-robin)
     2. Locks the mutex → updates shared counter
     3. Unlocks → passes turn to next thread
*/

void *thread_task(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    int id = args->thread_id;

    for (int round = 0; round < TOTAL_WORK; round++) {

        // ROUND-ROBIN: wait for our turn 
        pthread_mutex_lock(&scheduler_mutex);
        while (current_turn != id) {
            // sleep and release the lock until signalled
            pthread_cond_wait(&turn_cond, &scheduler_mutex);
        }
        pthread_mutex_unlock(&scheduler_mutex);

        // SIMULATE WORK (the thread "runs" for TIME_SLICE seconds)
        printf("[Thread %d] Round %d — working for %d second(s)...\n",
               id, round + 1, TIME_SLICE);
        sleep(TIME_SLICE);

        // UPDATE SHARED COUNTER SAFELY 
        pthread_mutex_lock(&counter_mutex);

        shared_counter++;
        printf("[Thread %d] Shared counter is now: %d\n", id, shared_counter);

        pthread_mutex_unlock(&counter_mutex);

        //PASS TURN TO NEXT THREAD 
        pthread_mutex_lock(&scheduler_mutex);
        current_turn = (current_turn + 1) % NUM_THREADS;
        pthread_cond_broadcast(&turn_cond); // wake all waiting threads
        pthread_mutex_unlock(&scheduler_mutex);
    }

    printf("[Thread %d] Finished all work.\n", id);
    return NULL;
}

// MAIN FUNCTION

int main(void)
{
    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    // INITIALISE MUTEX AND CONDITION VARIABLE
    pthread_mutex_init(&counter_mutex,   NULL);
    pthread_mutex_init(&scheduler_mutex, NULL);
    pthread_cond_init(&turn_cond,        NULL);

    printf("=== Round-Robin Thread Scheduler ===\n");
    printf("Threads: %d | Time slice: %ds | Rounds each: %d\n\n",
           NUM_THREADS, TIME_SLICE, TOTAL_WORK);

    // PAWN ALL THREADS 
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        if (pthread_create(&threads[i], NULL, thread_task, &args[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
        printf("[Main] Thread %d spawned.\n", i);
    }

    // WAIT FOR ALL THREADS TO FINISH
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // FINAL RESULT 
    printf("\n=== All threads completed ===\n");
    printf("Final shared counter value: %d\n", shared_counter);
    printf("Expected: %d (= %d threads × %d rounds)\n",
           NUM_THREADS * TOTAL_WORK, NUM_THREADS, TOTAL_WORK);

    // CLEANUP 
    pthread_mutex_destroy(&counter_mutex);
    pthread_mutex_destroy(&scheduler_mutex);
    pthread_cond_destroy(&turn_cond);

    return 0;
}