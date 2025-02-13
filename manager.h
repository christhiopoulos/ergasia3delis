#ifndef MANAGER_H
#define MANAGER_H

#include <sys/types.h>
#include <semaphore.h>
#include <signal.h>
#include "shared.h" // Make sure this is the correct path to "shared.h"

// Extern declarations for global variables
extern pid_t      receptionist_pid;
extern pid_t     *visitor_pids;
extern pid_t      monitor_pid;
extern volatile sig_atomic_t shutdown_requested;


// Function prototypes
void cleanup(SharedData *shdata, sem_t *sem_mutex, sem_t *sem_order_queue, const char *shm_name);
void sigint_handler();

#endif // MANAGER_H
