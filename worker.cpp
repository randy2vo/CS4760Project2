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

if(argc != 3) { 
	cerr << "Usage: ./worker <seconds> <nanoseconds>\n";
	return 1;
}

unsigned int intervalS = (unsigned int)strtoul(argv[1], nullptr, 10);
unsigned int intervalNS = (unsigned int)strtoul(argv[2], nullptr, 10);

cout << "Worker starting, PID:" << getpid() << " PPID:" << getppid() << "\n";
cout << "Called with\n";
cout << "Interval: " << intervalS << " seconds, " << intervalNS << " nanoseconds\n;


key_t key = ftok(".", 'C');
    if (key == -1) {
        cerr << "ftok failed: " << strerror(errno) << "\n";
        return 1;
    }

    int shmid = shmget(key, sizeof(SimClock), 0666);
    if (shmid == -1) {
        cerr << "shmget failed: " << strerror(errno) << "\n";
        return 1;
    }

    auto* clk = (SimClock*)shmat(shmid, nullptr, 0);
    if (clk == (void*)-1) {
        cerr << "shmat failed: " << strerror(errno) << "\n";
        return 1;
    }

    unsigned int startS  = clk->seconds;
    unsigned int startNS = clk->nanoseconds;

    unsigned int termS = 0, termNS = 0;
    addTime(startS, startNS, intervalS, intervalNS, termS, termNS);

    cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
    cout << "SysClockS: " << startS
         << " SysclockNano: " << startNS
         << " TermTimeS: " << termS
         << " TermTimeNano: " << termNS << "\n";
    cout << "--Just Starting\n";

    unsigned int lastSecond = startS;
    unsigned int passed = 0;

    // NO sleeping — busy-check simulated clock
    while (true) {
        unsigned int curS  = clk->seconds;
        unsigned int curNS = clk->nanoseconds;

        if (curS != lastSecond) {
            passed += (curS - lastSecond);
            lastSecond = curS;

            cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            cout << "SysClockS: " << curS
                 << " SysclockNano: " << curNS
                 << " TermTimeS: " << termS
                 << " TermTimeNano: " << termNS << "\n";
            cout << "--" << passed << " seconds have passed since starting\n";
        }

        if (reached(curS, curNS, termS, termNS)) {
            cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            cout << "SysClockS: " << curS
                 << " SysclockNano: " << curNS
                 << " TermTimeS: " << termS
                 << " TermTimeNano: " << termNS << "\n";
            cout << "--Terminating\n";
            break;
        }
    }

    shmdt(clk);
    return 0;
}







}

