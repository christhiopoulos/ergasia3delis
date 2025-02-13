CC = gcc
CFLAGS = -Wall -Wextra -lrt -g

# Τα εκτελέσιμα προγράμματα που θέλουμε να παράγουμε
TARGETS = manager receptionist visitor monitor

all: $(TARGETS)

# 2. receptionist
receptionist: receptionist.o shared.o receptionist.h shared.h
	$(CC) $(CFLAGS) receptionist.o shared.o -o receptionist

# 3. visitor
visitor: visitor.o shared.o visitor.h shared.h
	$(CC) $(CFLAGS) visitor.o shared.o -o visitor

# 4. monitor (αν υπάρχει)
monitor: monitor.o shared.o monitor.h shared.h
	$(CC) $(CFLAGS) monitor.o shared.o -o monitor

# Manager
manager: manager.o shared.o manager.h shared.h
	$(CC) $(CFLAGS) manager.o shared.o -o manager

# Κανόνες για τη μεταγλώττιση των .o

receptionist.o: receptionist.c receptionist.h shared.h
	$(CC) $(CFLAGS) -c receptionist.c -o receptionist.o

visitor.o: visitor.c visitor.h shared.h
	$(CC) $(CFLAGS) -c visitor.c -o visitor.o

monitor.o: monitor.c monitor.h shared.h
	$(CC) $(CFLAGS) -c monitor.c -o monitor.o

manager.o: manager.c manager.h shared.h
	$(CC) $(CFLAGS) -c manager.c -o manager.o

shared.o: shared.c shared.h
	$(CC) $(CFLAGS) -c shared.c -o shared.o

# Κανόνας clean
clean:
	rm -f *.o $(TARGETS)
