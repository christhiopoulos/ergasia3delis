// visitor.c
#include "visitor.h"


static SharedData *shdata = NULL;
static sem_t *sem_mutex = NULL;
static sem_t *sem_order_queue = NULL;

// cleanup
static void cleanup(void) {
    if (sem_mutex) sem_close(sem_mutex);
    if (sem_order_queue) sem_close(sem_order_queue);
    if (shdata) munmap(shdata, sizeof(SharedData));
}

static void handle_sigint(int sig) {
    (void)sig;
    
    cleanup();
    exit(0);
}

// Βρίσκουμε (αν υπάρχει) τραπέζι μη-full
static int find_table_seat(SharedData *shdata, pid_t mypid, double *wait_time_out) {
    // Χρονική σημείωση για μέτρηση αναμονής
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        // Βρίσκουμε τραπέζι
        int table_chosen = -1;
        for (int t=0; t<TABLES; t++) {
            if (!shdata->tables[t].table_full && shdata->tables[t].occupied < CHAIRS_PER_TABLE) {
                table_chosen = t;
                break;
            }
        }
        if (table_chosen != -1) {
            // Βρίσκουμε θέση
            int seat_pos = -1;
            for (int c=0; c<CHAIRS_PER_TABLE; c++) {
                if (shdata->tables[table_chosen].pids[c] == 0) {
                    seat_pos = c;
                    break;
                }
            }
            if (seat_pos != -1) {
                // Καταλαμβάνουμε θέση
                shdata->tables[table_chosen].pids[seat_pos] = mypid;
                shdata->tables[table_chosen].occupied++;
                shdata->tables[table_chosen].active++;
                if (shdata->tables[table_chosen].occupied == CHAIRS_PER_TABLE) {
                    shdata->tables[table_chosen].table_full = 1;
                }
                // Υπολογίζουμε τον actual wait time
                clock_gettime(CLOCK_MONOTONIC, &end);
                double w = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
                *wait_time_out = w;


                return table_chosen; // Επιστρέφουμε ποιο τραπέζι
            }
        }

        // Αν δεν βρέθηκε, κάνουμε sleep(1) και ξαναδοκιμάζουμε
        sem_post(sem_mutex); // Αποδεσμεύουμε το mutex για να μην μπλοκάρουμε
        sleep(1);
        sem_wait(sem_mutex);
        if (!shdata->running) {
            // Σταματάμε
            return -1;
        }
    }
}

int main(int argc, char *argv[]) {
    char *shm_name = NULL;
    double rest_time = 10.0;

    // Parse args
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-s")) {
            shm_name = argv[++i];
        } else if (!strcmp(argv[i], "-d")) {
            rest_time = atof(argv[++i]);
        }
    }

    if (!shm_name) {
        fprintf(stderr, "Usage: %s -s <shm_name> [-d rest_time]\n", argv[0]);
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    // Σύνδεση στο shared memory
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(1);
    }
    shdata = mmap(NULL, sizeof(SharedData), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shdata == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        exit(1);
    }
    close(shm_fd);

    // Άνοιγμα semaphores
    sem_mutex = sem_open(SEM_MUTEX, 0);
    if (sem_mutex == SEM_FAILED) {
        perror("sem_open /bar_mutex");
        cleanup();
        exit(1);
    }
    sem_order_queue = sem_open(SEM_ORDER_QUEUE, 0);
    if (sem_order_queue == SEM_FAILED) {
        perror("sem_open /bar_order_queue");
        cleanup();
        exit(1);
    }

    srand(time(NULL) ^ (getpid()<<16));
    

    // 1) Βρίσκουμε τραπέζι
    sem_wait(sem_mutex);
    if (!shdata->running) {
        sem_post(sem_mutex);
        cleanup();
        return 0;
    }
    double wait_table=0.0;
    int table_chosen = find_table_seat(shdata, getpid(), &wait_table);
    if (table_chosen == -1) {
        sem_post(sem_mutex);
        cleanup();
        return 0;
    }
    // 2) Τοποθέτηση παραγγελίας (μία φορά)
    //    Δημιουργία μοναδικού semaphore
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "/order_done_%d", getpid());

    sem_t *sem_order_done = sem_open(sem_name, O_CREAT, 0666, 0);
    if (sem_order_done == SEM_FAILED) {
        perror("visitor sem_open unique");
        // Ελευθερώνουμε θέση τραπεζιού
        cleanup();
        return 1;
    }

    // Φτιάχνουμε order
    Order my_order;
    my_order.pid = getpid();
    strncpy(my_order.sem_name, sem_name, sizeof(my_order.sem_name)-1);
    my_order.sem_name[sizeof(my_order.sem_name)-1] = '\0';

    // Ελέγχουμε ουρά γεμάτη;
    int next_tail = (shdata->tail+1) % MAX_ORDERS;
    if (next_tail == shdata->head) {
        // Ουρά γεμάτη
    
        // Ελευθερώνουμε θέση τραπεζιού
        for (int c=0; c<CHAIRS_PER_TABLE; c++) {
            if (shdata->tables[table_chosen].pids[c] == getpid()) {
                shdata->tables[table_chosen].pids[c] = 0;
                shdata->tables[table_chosen].occupied--;
                shdata->tables[table_chosen].active--;
                // Τροποποιημένη συνθήκη για αποδέσμευση table_full
                if (shdata->tables[table_chosen].table_full == 1 &&
                    shdata->tables[table_chosen].occupied == 0) {
                    shdata->tables[table_chosen].table_full = 0;
                }
                break;
            }
        }
        sem_post(sem_mutex);
        sem_close(sem_order_done);

        cleanup();
        return 1;
    }

    // enqueue
    shdata->orders[shdata->tail] = my_order;
    shdata->tail = next_tail;

    // Στατιστικά: wait_time += wait_table
    shdata->total_wait_time += wait_table;

    sem_post(sem_mutex);

    // Σήμα ότι έχουμε νέα παραγγελία
  
    sem_post(sem_order_queue);

    // 3) Περιμένουμε ολοκλήρωση
    sem_wait(sem_order_done);
    sem_close(sem_order_done);



    // 4) Ενημέρωση στατιστικών (στο mutex)
    sem_wait(sem_mutex);
    shdata->total_visitors++;
    // shdata->total_wait_time += wait_table; // Προστέθηκε παραπάνω

    // Προαιρετικά: τυχαία κατανάλωση
    int water = rand()%2;
    int wine  = rand()%2;
    int cheese= rand()%2;
    int salad = rand()%2;
    if (!water && !wine) {
    // Τυχαία επιλογή να αγοράσει είτε νερό είτε κρασί
    if (rand() % 2) {
        water = 1;
    } else {
        wine = 1;
    }
}
    shdata->total_water  += water;
    shdata->total_wine   += wine;
    shdata->total_cheese += cheese;
    shdata->total_salad  += salad;

    sem_post(sem_mutex);

    // 5) Κάθεται/ξεκουράζεται (rest_time)
    double stay_duration = (0.7*rest_time) + ((double)rand() / RAND_MAX)*(rest_time-0.7*rest_time);
    usleep((useconds_t)(stay_duration * 1e6));

    sem_wait(sem_mutex);

    // 6) Αποχώρηση από το τραπέζι
    shdata->total_stay_time += stay_duration;

    // Βρίσκουμε σε ποια θέση καθόμασταν:
    int seat_pos = -1;
    for (int c=0; c<CHAIRS_PER_TABLE; c++) {
        if (shdata->tables[table_chosen].pids[c] == getpid()) {
            seat_pos = c;
            break;
        }
    }
    if (seat_pos >=0) {
        shdata->tables[table_chosen].pids[seat_pos] = 0;
        shdata->tables[table_chosen].active--;
        shdata->tables[table_chosen].occupied--;
        // Τροποποιημένη συνθήκη για αποδέσμευση table_full
        if (shdata->tables[table_chosen].table_full == 1 &&
            shdata->tables[table_chosen].occupied == 0) {
            shdata->tables[table_chosen].table_full = 0;
        }
    }
    sem_post(sem_mutex);


        

    cleanup();
    return 0;
}
