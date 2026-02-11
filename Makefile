CC=g++
CFLAGS=-Wall -Wextra -02 -std=c++17

all: oss worker

oss: oss.cpp
	$(CC) $(CFLAGS) -o oss oss.cpp

worker: worker.cpp
	$(CC) $(CFLAGS) -o worker.cpp


clean:
	rm -f oss worker *.o


