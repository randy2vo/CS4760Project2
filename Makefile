
all: oss worker

oss: oss.cpp
	g++ -Wall  -o oss oss.cpp

worker: worker.cpp
	g++ -Wall -o worker worker.cpp


clean:
	rm -f oss worker *.o


