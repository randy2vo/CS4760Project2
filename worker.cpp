#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include<unistd.h>
#include<sys/ipc.h>
#include<sys/shm.h>

using namespace std;

struct SimClock {
	
	unsigned int seconds;
        unsigned int nanoseconds;
};

static void addTime(unsigned int baseS, unsigned int baseNS, unsigned int addS, unsigned int addNS, unsigned int &outS, unsigned int &outNS) {

	unsigned long long ns = (unsigned long long)baseNS + (unsigned long long)addNS;
	outS = baseS + addS + (unsigned int)(ns/ 1000000000ULL);
	outNS = (unsigned int)(ns % 1000000000ULL);
}

static bool reached(unsigned int s, unsigned int ns, unsigned int tS, unsigned int tNS) {
	return (s > tS) || (s == tS && ns >= tNS);
}

int main (int argc, char* argv[]) {










}

