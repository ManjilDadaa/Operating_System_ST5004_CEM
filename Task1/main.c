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
