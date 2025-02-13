#ifndef MONITOR_H
#define MONITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

#include "shared.h"

// Ορισμοί των semaphores (ίδιοι με αυτούς στο manager.c)
#define SEM_MUTEX        "/bar_mutex"
#define SEM_ORDER_QUEUE  "/bar_order_queue"

// Global flag για το SIGINT handling
extern volatile sig_atomic_t keep_running;

// Prototype του χειριστή σήματος
void sigint_handler(int signum);

#endif // MONITOR_H
