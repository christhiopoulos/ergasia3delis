// shared.h
#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>
#include <sys/types.h>

#define MAX_ORDERS 10000   // Μέγιστος αριθμός παραγγελιών (queue size)
#define TABLES 3            // Αριθμός τραπεζιών
#define CHAIRS_PER_TABLE 4  // Αριθμός καθισμάτων ανά τραπέζι

// Δομή για μία παραγγελία
typedef struct {
    pid_t pid;                // PID του Visitor
    char sem_name[256];       // Όνομα του μοναδικού semaphore για την παραγγελία
} Order;

// Δομή για ένα τραπέζι
typedef struct {
    int occupied;             // Πόσες καρέκλες είναι κατειλημμένες
    int active;               // Πόσοι visitors είναι ενεργοί αυτή τη στιγμή
    int table_full;           // 1 = τραπέζι γεμάτο (4 άτομα), 0 = όχι
    pid_t pids[CHAIRS_PER_TABLE]; // PID των Visitors που κάθονται στο τραπέζι
} Table;

// Δομή για τα κοινόχρηστα δεδομένα
typedef struct {
    // Ουρά παραγγελιών
    Order orders[MAX_ORDERS];
    int head;      // δείκτης κεφαλής
    int tail;      // δείκτης ουράς
    int running;   // 1 = λειτουργεί, 0 = τερματισμός

    // Στατιστικά
    int total_visitors;
    double total_wait_time;
    double total_stay_time;
    int total_water;
    int total_wine;
    int total_cheese;
    int total_salad;

    // Τραπέζια
    Table tables[TABLES];
} SharedData;
int init_shared_memory(const char *shm_name, int create, SharedData **shdata);
#endif // SHARED_H