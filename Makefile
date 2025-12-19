CC=gcc
CFLAGS=-Wall -Wextra -pthread

TARGET=balancer_lc
SRC=src/balancer.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
