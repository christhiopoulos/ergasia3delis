
#include "monitor.h"

// Use the same semaphore names as in manager.c


 volatile sig_atomic_t keep_running=1;

void sigint_handler(int signum) {
    (void)signum; // unused
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    // Default shared memory name
    const char *shm_name = "/shmid";
    
    // Parse CLI args
    int opt;
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
            case 's':
                shm_name = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-s shm_name]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    // Setup SIGINT handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("monitor: sigaction");
        exit(EXIT_FAILURE);
    }
    
    // Open the shared memory
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("monitor: shm_open");
        exit(EXIT_FAILURE);
    }

    // Map the shared memory
    SharedData *shdata = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shdata == MAP_FAILED) {
        perror("monitor: mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Open semaphores
    sem_t *sem_mutex = sem_open(SEM_MUTEX, 0);
    if (sem_mutex == SEM_FAILED) {
        perror("monitor: sem_open(mutex)");
        munmap(shdata, sizeof(SharedData));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_order_queue = sem_open(SEM_ORDER_QUEUE, 0);
    if (sem_order_queue == SEM_FAILED) {
        perror("monitor: sem_open(order_queue)");
        sem_close(sem_mutex);
        munmap(shdata, sizeof(SharedData));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("Monitor: Started. Monitoring every 2 seconds...\n");

    // Main monitoring loop
    while (keep_running) {
        // Sleep 2 seconds between reports
        sleep(2);

        // Attempt to acquire the mutex
        if (sem_wait(sem_mutex) < 0) {
            if (errno == EINTR && !keep_running) {
                // Interrupted by signal, and we're exiting
                break;
            }
            perror("monitor: sem_wait(mutex)");
            break;
        }

        // ---------------
        // PRINT REPORT
        // ---------------
        printf("\n=== Monitor Report ===\n");
        
        printf("Total visitors so far: %d\n", shdata->total_visitors);
        printf("Water orders:  %d\n", shdata->total_water);
        printf("Wine orders:   %d\n", shdata->total_wine);
        printf("Cheese orders: %d\n", shdata->total_cheese);
        printf("Salad orders:  %d\n", shdata->total_salad);

        double avg_wait = (shdata->total_visitors > 0)
                            ? (shdata->total_wait_time / shdata->total_visitors)
                            : 0.0;
        double avg_stay = (shdata->total_visitors > 0)
                            ? (shdata->total_stay_time / shdata->total_visitors)
                            : 0.0;
        printf("Average wait time: %.2f\n", avg_wait);
        printf("Average stay time: %.2f\n", avg_stay);

        // Print occupancy for each table
        for (int t = 0; t < TABLES; t++) {
            printf("\nTable %d:\n", t);
            printf("  occupied=%d, active=%d, full=%d\n",
                   shdata->tables[t].occupied,
                   shdata->tables[t].active,
                   shdata->tables[t].table_full);

            // Print each seat's occupant PID
            for (int c = 0; c < CHAIRS_PER_TABLE; c++) {
                pid_t occupant = shdata->tables[t].pids[c];
                if (occupant > 0) {
                    printf("    Chair %d has PID %d\n", c, occupant);
                } else {
                    printf("    Chair %d is empty\n", c);
                }
            }
        }

        // Release the mutex
        if (sem_post(sem_mutex) < 0) {
            perror("monitor: sem_post(mutex)");
            break;
        }
    }

    // Cleanup
    printf("Monitor: Shutting down...\n");
    sem_close(sem_mutex);
    sem_close(sem_order_queue);
    munmap(shdata, sizeof(SharedData));
    close(shm_fd);
    return 0;
}
