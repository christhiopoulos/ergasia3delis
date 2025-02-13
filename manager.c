// manager.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>

#include "shared.h"

#define SEM_MUTEX        "/bar_mutex"
#define SEM_ORDER_QUEUE  "/bar_order_queue"

pid_t receptionist_pid = -1;
pid_t *visitor_pids = NULL; // Δυναμικός πίνακας για visitor PIDs
int num_visitors = 25; // Αρχική τιμή, μπορεί να αλλάξει μέσω γραμμής εντολών
pid_t monitor_pid = -1;
static double order_time = 5.0;
static double rest_time  = 5.0;

// Global flag for shutdown
volatile sig_atomic_t shutdown_requested = 0;

// Συνάρτηση για cleanup
void cleanup(SharedData *shdata, sem_t *sem_mutex, sem_t *sem_order_queue, const char *shm_name) {
    // Κλείσιμο semaphores
    if (sem_mutex) sem_close(sem_mutex);
    if (sem_order_queue) sem_close(sem_order_queue);

    // Unlink semaphores
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_ORDER_QUEUE);

    // Αποσύνδεση shared memory
    if (shdata) munmap(shdata, sizeof(SharedData));

    // Unlink shared memory
    shm_unlink(shm_name);

    // Απελευθέρωση μνήμης για visitor_pids
    if (visitor_pids) free(visitor_pids);
}

// Handler για SIGINT που θέτει το flag
void sigint_handler() {
    shutdown_requested = 1;
}

int main(int argc, char *argv[]) {
    // Ορισμός shared memory name
    const char *shm_name = "/shmid";

    // Ανάλυση παραμέτρων γραμμής εντολών
       int opt;
    while ((opt = getopt(argc, argv, "o:r:v:")) != -1) {
        switch (opt) {
            case 'o':
                order_time = atof(optarg);
                if (order_time <= 0) {
                    fprintf(stderr, "manager: order_time must be positive.\n");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'r':
                rest_time = atof(optarg);
                if (rest_time <= 0) {
                    fprintf(stderr, "manager: rest_time must be positive.\n");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'v':
                num_visitors = atoi(optarg);
                if (num_visitors <= 0) {
                    fprintf(stderr, "manager: num_visitors must be positive.\n");
                    exit(EXIT_FAILURE);
                }
                break;

            default:
                fprintf(stderr, "Usage: %s [-o order_time] [-r rest_time] [-v num_visitors]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Δυναμική κατανομή μνήμης για visitor_pids
    visitor_pids = malloc(sizeof(pid_t) * num_visitors);
    if (visitor_pids == NULL) {
        perror("manager: malloc");
        exit(1);
    }
    memset(visitor_pids, 0, sizeof(pid_t) * num_visitors); // Αρχικοποίηση σε 0

    // Δημιουργία και αρχικοποίηση shared memory
    SharedData *shdata;
    if (init_shared_memory(shm_name, 1, &shdata) < 0) {
        fprintf(stderr, "manager: Could not create shared memory %s\n", shm_name);
        free(visitor_pids);
        exit(1);
    }
    

    // Δημιουργία και αρχικοποίηση semaphores
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_ORDER_QUEUE);

    sem_t *sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
    if (sem_mutex == SEM_FAILED) {
        perror("manager: sem_open /bar_mutex");
        cleanup(shdata, NULL, NULL, shm_name);
        exit(1);
    }

    sem_t *sem_order_queue = sem_open(SEM_ORDER_QUEUE, O_CREAT, 0666, 0);
    if (sem_order_queue == SEM_FAILED) {
        perror("manager: sem_open /bar_order_queue");
        cleanup(shdata, sem_mutex, NULL, shm_name);
        exit(1);
    }

    // Αρχικοποίηση SharedData
    sem_wait(sem_mutex);
    // Αρχικοποίηση τραπεζιών
    for (int t = 0; t < TABLES; t++) {
        shdata->tables[t].occupied = 0;
        shdata->tables[t].active = 0;
        shdata->tables[t].table_full = 0;
        for (int c = 0; c < CHAIRS_PER_TABLE; c++) {
            shdata->tables[t].pids[c] = 0;
        }
    }
    shdata->running = 1;
    shdata->total_visitors = 0;
    shdata->total_water = 0;
    shdata->total_wine = 0;
    shdata->total_cheese = 0;
    shdata->total_salad = 0;
    shdata->total_wait_time = 0.0;
    shdata->total_stay_time = 0.0;
    sem_post(sem_mutex);

    

    // Ρύθμιση handler για SIGINT
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Αφαιρέστε το SA_RESTART
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("manager: sigaction");
        cleanup(shdata, sem_mutex, sem_order_queue, shm_name);
        exit(1);
    }

    // Εκκίνηση του receptionist
    receptionist_pid = fork();
    if (receptionist_pid < 0) {
        perror("manager: fork receptionist");
        cleanup(shdata, sem_mutex, sem_order_queue, shm_name);
        exit(1);
    }
    if (receptionist_pid == 0) {
        // Child process: exec receptionist
        execl("./receptionist", "./receptionist",
              "-s", shm_name,
              "-d", "5",
              (char *)NULL);
        perror("manager: execl receptionist");
        exit(1);
    }
    

    // Εκκίνηση του monitor
    monitor_pid = fork();
    if (monitor_pid < 0) {
        perror("manager: fork monitor");
        cleanup(shdata, sem_mutex, sem_order_queue, shm_name);
        exit(1);
    }
    if (monitor_pid == 0) {
        // Child process: exec monitor
        execl("./monitor", "./monitor",
              "-s", shm_name,
              (char *)NULL);
        perror("manager: execl monitor");
        exit(1);
    }
   

    // Εκκίνηση των visitors
    for (int i = 0; i < num_visitors; i++) {
        pid_t vpid = fork();
        if (vpid < 0) {
            perror("manager: fork visitor");
            continue;
        }
        if (vpid == 0) {
            // Child process: exec visitor
            execl("./visitor", "./visitor",
                  "-s", shm_name,
                  "-d", "5",
                  (char *)NULL);
            perror("manager: execl visitor");
            exit(1);
        }
        visitor_pids[i] = vpid;
    }

    // Κύριος βρόχος που περιμένει τα child processes και ελέγχει το shutdown flag
while (1) {
    int status;
    pid_t wpid = waitpid(-1, &status, 0);
    if (wpid == -1) {
        if (errno == EINTR) {
            // Possibly a shutdown signal or something else
            if (shutdown_requested) {
                // Clean shutdown logic, e.g., kill children, etc.
                sem_wait(sem_mutex);
                shdata->running = 0;
                sem_post(sem_mutex);

                if (receptionist_pid > 0) {
                    kill(receptionist_pid, SIGINT);
                }
                for (int i = 0; i < num_visitors; i++) {
                    if (visitor_pids[i] > 0) {
                        kill(visitor_pids[i], SIGINT);
                    }
                }
                if (monitor_pid > 0) {
                    kill(monitor_pid, SIGINT);
                }
            }
            continue;
        } else if (errno == ECHILD) {
            // No more child processes
            break;
        } else {
            perror("manager: waitpid");
            break;
        }
    }

    // wpid >= 0 means a child process exited
    // We can check the exit status if we want:
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("manager: child %d exited with code %d\n", wpid, exit_code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("manager: child %d killed by signal %d\n", wpid, sig);
    }

    if (wpid == receptionist_pid) {
        // The Receptionist exited
        printf("manager: Receptionist [%d] exited.\n", wpid);

        // Possibly set running=0 or do partial shutdown
        sem_wait(sem_mutex);
        shdata->running = 0;
        sem_post(sem_mutex);

        // Kill all other children (Visitors + Monitor)
        for (int i = 0; i < num_visitors; i++) {
            if (visitor_pids[i] > 0) {
                kill(visitor_pids[i], SIGINT);
            }
        }
        if (monitor_pid > 0) {
            kill(monitor_pid, SIGINT);
        }
    }
    else if (wpid == monitor_pid) {
        // The Monitor exited
        printf("manager: Monitor [%d] exited.\n", wpid);


    }
    else {
        // Probably one of the Visitors
        for (int i = 0; i < num_visitors; i++) {
            if (visitor_pids[i] == wpid) {
                printf("manager: Visitor [%d] exited.\n", wpid);
                // Mark that visitor as "finished"
                visitor_pids[i] = -1; 
                break;
            }
        }
    }
} 


    // Καθαρισμός πόρων
    cleanup(shdata, sem_mutex, sem_order_queue, shm_name);
    
    return 0;
}
