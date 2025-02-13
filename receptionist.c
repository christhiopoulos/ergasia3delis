
#include "receptionist.h"


static SharedData *shdata = NULL;
static sem_t *sem_mutex = NULL;
static sem_t *sem_order_queue = NULL;

// Cleanup function
static void cleanup(void) {
    if (sem_mutex) sem_close(sem_mutex);
    if (sem_order_queue) sem_close(sem_order_queue);
    if (shdata) {
        munmap(shdata, sizeof(SharedData));
    }


}

static void handle_sigint(int sig) {
    (void)sig;
   

    // Σηματοδοτούμε τερματισμό
    sem_wait(sem_mutex);
    shdata->running = 0;
    sem_post(sem_mutex);

    // Εκτύπωση στατιστικών
    double avg_wait = 0.0;
    double avg_stay = 0.0;
    if (shdata->total_visitors > 0) {
        avg_wait = shdata->total_wait_time / shdata->total_visitors;
        avg_stay = shdata->total_stay_time / shdata->total_visitors;
    }
    printf("=== Statistics ===\n");
    printf("Total Visitors: %d\n", shdata->total_visitors);
    printf("Average Wait Time: %.2f sec\n", avg_wait);
    printf("Average Stay Time: %.2f sec\n", avg_stay);
    printf("Total Water: %d\n", shdata->total_water);
    printf("Total Wine: %d\n", shdata->total_wine);
    printf("Total Cheese: %d\n", shdata->total_cheese);
    printf("Total Salad: %d\n", shdata->total_salad);

    printf("Receptionist terminated.\n");
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    double service_time = 5.0;
    char *shm_name = "/shmid";

    // Ανάγνωση παραμέτρων
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            service_time = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-s")) {
            shm_name = argv[++i];
        }
    }

    // Ρύθμιση signal handler
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    // Δημιουργία/άνοιγμα shared memory
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(SharedData)) < 0) {
        perror("ftruncate");
        close(shm_fd);
        exit(1);
    }

    shdata = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shdata == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        exit(1);
    }
    close(shm_fd);



    sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
    if (sem_mutex == SEM_FAILED) {
        perror("sem_open /bar_mutex");
        cleanup();
        exit(1);
    }

    sem_order_queue = sem_open(SEM_ORDER_QUEUE, O_CREAT, 0666, 0);
    if (sem_order_queue == SEM_FAILED) {
        perror("sem_open /bar_order_queue");
        cleanup();
        exit(1);
    }

    // Αρχικοποίηση
    sem_wait(sem_mutex);
    shdata->running = 1;
    shdata->head = 0;
    shdata->tail = 0;
    shdata->total_visitors = 0;
    shdata->total_wait_time = 0.0;
    shdata->total_stay_time = 0.0;
    shdata->total_water = 0;
    shdata->total_wine = 0;
    shdata->total_cheese = 0;
    shdata->total_salad = 0;
    // Clear orders
    for (int i = 0; i < MAX_ORDERS; i++) {
        shdata->orders[i].pid = 0;
        shdata->orders[i].sem_name[0] = '\0';
    }
    // Clear tables
    for (int t = 0; t < TABLES; t++) {
        shdata->tables[t].occupied = 0;
        shdata->tables[t].active = 0;
        shdata->tables[t].table_full = 0;
        for (int c = 0; c < CHAIRS_PER_TABLE; c++) {
            shdata->tables[t].pids[c] = 0;
        }
    }
    sem_post(sem_mutex);

   

    srand(time(NULL));

    // Κυρίως βρόχος εξυπηρέτησης
    while (1) {
        // Περιμένουμε παραγγελία
        sem_wait(sem_order_queue);

        sem_wait(sem_mutex);
        if (!shdata->running) {
            sem_post(sem_mutex);
            break;
        }

        // Διαβάζουμε από την ουρά
        int h = shdata->head;
        if (shdata->orders[h].pid == 0) {
            // Άδεια παραγγελία
            sem_post(sem_mutex);
            continue;
        }

   
        char semname[256];
        strcpy(semname, shdata->orders[h].sem_name);

        // Clear order slot
        shdata->orders[h].pid = 0;
        shdata->orders[h].sem_name[0] = '\0';
        shdata->head = (h + 1) % MAX_ORDERS;

        sem_post(sem_mutex);

        // Υπολογισμός τυχαίας διάρκειας εξυπηρέτησης [0.50 * service_time .. service_time]
        double actual_service_time = 0.5 * service_time + ((double)rand() / RAND_MAX) * (0.5 * service_time);
        
        usleep((useconds_t)(actual_service_time * 1e6)); // usleep δέχεται μικρότερες μονάδες χρόνου

        
        // Ανοίγουμε το semaphore του επισκέπτη
        sem_t *sem_order_done = sem_open(semname, 0);
        if (sem_order_done == SEM_FAILED) {
            perror("sem_open visitor sem");
            continue;
        }
        sem_post(sem_order_done);
        sem_close(sem_order_done);
        
    }

    // Cleanup στο τέλος
    handle_sigint(0);
    return 0;
}