#ifndef RECEPTIONIST_H
#define RECEPTIONIST_H

#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "shared.h"

/* 
 * Ορισμοί για τα ονόματα των semaphores
 * (ίδιοι με αυτούς στο receptionist.c)
 */
#define SEM_MUTEX        "bar_mutex"
#define SEM_ORDER_QUEUE  "bar_order_queue"



#endif // RECEPTIONIST_H
