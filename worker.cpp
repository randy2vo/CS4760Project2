#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
using namespace std;

struct SimClock {
    unsigned int seconds;
    unsigned int nanoseconds;
};


/*total seconds = base seconds + added seconds + whatever seconds within nanoseconds ex. if ns = 1,500,000,000 then 
  1,500,000,000 / 1,000,000,000 = 1 extra second within nanoseconds.
  */

static void addTime(unsigned int baseS,  unsigned int baseNS,
                    unsigned int addS,   unsigned int addNS,
                    unsigned int &outS,  unsigned int &outNS) {
    unsigned long long ns = (unsigned long long)baseNS + (unsigned long long)addNS;
    outS  = baseS + addS + (unsigned int)(ns / 1000000000ULL);
    outNS = (unsigned int)(ns % 1000000000ULL);
}

static bool reached(unsigned int s,  unsigned int ns,
                    unsigned int tS, unsigned int tNS) {
    return (s > tS) || (s == tS && ns >= tNS);
}

int main(int argc, char* argv[]) {

    if (argc != 3) {
        cerr << "Usage: ./worker <seconds> <nanoseconds>\n";
        return 1;
    }

    unsigned int intervalS  = (unsigned int)strtoul(argv[1], nullptr, 10);
    unsigned int intervalNS = (unsigned int)strtoul(argv[2], nullptr, 10);

    cout << "Worker starting, PID:" << getpid() << " PPID:" << getppid() << "\n";
    cout << "Called with:\n";
    cout << "Interval: " << intervalS << " seconds, " << intervalNS << " nanoseconds\n";

    // Attach to shared memory — key must match oss.cpp exactly
    key_t key = ftok(".", 'C');
    if (key == -1) {
        cerr << "Worker: ftok failed: " << strerror(errno) << "\n";
        return 1;
    }

    int shmid = shmget(key, sizeof(SimClock), 0666);
    if (shmid == -1) {
        cerr << "Worker: shmget failed: " << strerror(errno) << "\n";
        return 1;
    }

    auto* clk = (SimClock*)shmat(shmid, nullptr, 0);
    if (clk == (SimClock*)-1) {
        cerr << "Worker: shmat failed: " << strerror(errno) << "\n";
        return 1;
    }

    // Snapshot start time and compute termination target
    unsigned int startS  = clk->seconds;
    unsigned int startNS = clk->nanoseconds;
    unsigned int termS   = 0;
    unsigned int termNS  = 0;
    addTime(startS, startNS, intervalS, intervalNS, termS, termNS);

    cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
    cout << "SysClockS: "   << startS  << " SysclockNano: " << startNS
         << " TermTimeS: "  << termS   << " TermTimeNano: " << termNS << "\n";
    cout << "--Just Starting\n";

    unsigned int lastSecond = startS;
    unsigned int passed     = 0;

    // Busy-wait — NO sleep
    while (true) {
        unsigned int curS  = clk->seconds;
        unsigned int curNS = clk->nanoseconds;

        // Print a message every time the simulated second changes
        if (curS != lastSecond) {
            passed    += (curS - lastSecond);
            lastSecond = curS;
            cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            cout << "SysClockS: "  << curS  << " SysclockNano: " << curNS
                 << " TermTimeS: " << termS << " TermTimeNano: " << termNS << "\n";
            cout << "--" << passed << " seconds have passed since starting\n";
        }

        // Check if termination time has been reached
        if (reached(curS, curNS, termS, termNS)) {
            cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            cout << "SysClockS: "  << curS  << " SysclockNano: " << curNS
                 << " TermTimeS: " << termS << " TermTimeNano: " << termNS << "\n";
            cout << "--Terminating\n";
            break;
        }
    }

    shmdt(clk);
    return 0;
}
