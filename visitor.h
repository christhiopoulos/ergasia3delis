#ifndef VISITOR_H
#define VISITOR_H

#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"

/* 
 * Ορισμοί για τα ονόματα των semaphores
 * (ίδιοι με αυτούς στο visitor.c).
 */
#define SEM_MUTEX        "/bar_mutex"
#define SEM_ORDER_QUEUE  "/bar_order_queue"


#endif // VISITOR_H
