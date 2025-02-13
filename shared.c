// shared.c
#include "shared.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int init_shared_memory(const char *shm_name, int create, SharedData **shdata) {
    int shm_fd;
    if (create) {
        shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (shm_fd < 0) {
            perror("shm_open");
            return -1;
        }
        // Αύξηση μεγέθους shared memory
        if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
            perror("ftruncate");
            close(shm_fd);
            return -1;
        }
    } else {
        shm_fd = shm_open(shm_name, O_RDWR, 0666);
        if (shm_fd < 0) {
            perror("shm_open");
            return -1;
        }
    }

    // Χαρτογράφηση της μνήμης
    void *ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    close(shm_fd);
    *shdata = (SharedData *)ptr;

    // Αν δημιουργούμε το shared memory, αρχικοποιούμε το running flag
    if (create) {
        (*shdata)->running = 1;
        // Αρχικοποίηση άλλων πεδίων αν χρειάζεται
    }

    return 0;
}