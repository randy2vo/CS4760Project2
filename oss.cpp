#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <signal.h>


using namespace std;


// ── Structures ────────────────────────────────────────────────────────────────
struct SimClock {
    unsigned int seconds;
    unsigned int nanoseconds;
};

struct PCB {
    int          occupied;
    pid_t        pid;
    unsigned int startSeconds;
    unsigned int startNano;
    unsigned int endingTimeSeconds;
    unsigned int endingTimeNano;
};

// ── Constants ─────────────────────────────────────────────────────────────────
const int TABLE_SIZE      = 20;
const int CLOCK_INCREMENT = 10000000; // 10ms in nanoseconds

// ── Globals (needed by signal handler) ───────────────────────────────────────
static int       g_shmid   = -1;
static SimClock* g_clk     = nullptr;
static PCB       g_table[TABLE_SIZE];

// ── Helpers ───────────────────────────────────────────────────────────────────
static void incrementClock(SimClock* clk, unsigned int addNS = CLOCK_INCREMENT) {
    clk->nanoseconds += addNS;
    if (clk->nanoseconds >= 1000000000U) {
        clk->seconds++;
        clk->nanoseconds -= 1000000000U;
    }
}

static bool timeGTE(unsigned int sA, unsigned int nA,
                    unsigned int sB, unsigned int nB) {
    return (sA > sB) || (sA == sB && nA >= nB);
}

static long long nanosBetween(unsigned int s1, unsigned int n1,
                               unsigned int s2, unsigned int n2) {
    return (long long)(s2 - s1) * 1000000000LL + (long long)n2 - (long long)n1;
}

// ── Process table print ───────────────────────────────────────────────────────
static void printProcessTable() {
    cout << "\nOSS PID:" << getpid()
         << " SysClockS: "    << g_clk->seconds
         << " SysclockNano: " << g_clk->nanoseconds << "\n"
         << "Process Table:\n"
         << "Entry\tOccupied\tPID\t\tStartS\tStartN\t\tEndingTimeS\tEndingTimeNano\n";

    for (int i = 0; i < TABLE_SIZE; i++) {
        PCB& p = g_table[i];
        if (p.occupied) {
            cout << i     << "\t1\t\t"
                 << p.pid << "\t\t"
                 << p.startSeconds << "\t"
                 << p.startNano    << "\t\t"
                 << p.endingTimeSeconds << "\t"
                 << p.endingTimeNano    << "\n";
        } else {
            cout << i << "\t0\t\t0\t\t0\t0\t\t0\t0\n";
        }
    }
    cout << "\n";
}

// ── Help ──────────────────────────────────────────────────────────────────────
static void printHelp(const char* prog) {
    cout << "Usage: " << prog
         << " [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i intervalToLaunch]\n"
         << "  -h       Show this help message and exit\n"
         << "  -n proc  Total number of children to launch (max 80)\n"
         << "  -s simul Max simultaneous children (max 15)\n"
         << "  -t secs  Simulated time limit passed to each worker (float, e.g. 2.5)\n"
         << "  -i secs  Minimum simulated interval between launches (float, e.g. 0.1)\n\n"
         << "Example:\n"
         << "  " << prog << " -n 5 -s 3 -t 4 -i 0.5\n";
}

// ── Signal handler ────────────────────────────────────────────────────────────
static void signal_handler(int sig) {
    cerr << "\nOSS: caught signal " << sig
         << ". Terminating children and cleaning up...\n";

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (g_table[i].occupied && g_table[i].pid > 0)
            kill(g_table[i].pid, SIGTERM);
    }

    // Reap children
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {}

    if (g_clk && g_clk != (SimClock*)-1) {
        shmdt(g_clk);
        g_clk = nullptr;
    }
    if (g_shmid != -1) {
        shmctl(g_shmid, IPC_RMID, nullptr);
        g_shmid = -1;
    }

    _exit(1);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {

    // Signals first
    signal(SIGINT,  signal_handler);
    signal(SIGALRM, signal_handler);
    alarm(60);

    // ── Parse args ────────────────────────────────────────────────────────────
    int   n        = 1;
    int   s        = 1;
    float t        = 1.0f;
    float interval = 0.1f;

    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (opt) {
            case 'h':
                printHelp(argv[0]);
                return 0;
            case 'n':
                n = atoi(optarg);
                if (n <= 0 || n > 80) { cerr << "Error: -n must be between 1 and 80\n"; return 1; }
                break;
            case 's':
                s = atoi(optarg);
                if (s <= 0 || s > 15) { cerr << "Error: -s must be between 1 and 15\n"; return 1; }
                break;
            case 't':
                t = atof(optarg);
                if (t <= 0) { cerr << "Error: -t must be positive\n"; return 1; }
                break;
            case 'i':
                interval = atof(optarg);
                if (interval < 0) { cerr << "Error: -i must be non-negative\n"; return 1; }
                break;
            default:
                printHelp(argv[0]);
                return 1;
        }
    }

    if (s > n) s = n;

    // Convert float t and interval to seconds + nanoseconds
    unsigned int tSec   = (unsigned int)t;
    unsigned int tNano  = (unsigned int)((t - (float)tSec) * 1e9f);
    unsigned int iSec   = (unsigned int)interval;
    unsigned int iNano  = (unsigned int)((interval - (float)iSec) * 1e9f);

    cout << "OSS starting, PID:" << getpid() << " PPID:" << getppid() << "\n"
         << "Called with:\n"
         << "  -n " << n << "\n"
         << "  -s " << s << "\n"
         << "  -t " << t << "\n"
         << "  -i " << interval << "\n\n";

    // ── Shared memory ─────────────────────────────────────────────────────────
    key_t key = ftok(".", 'C');   // must match worker.cpp
    if (key == -1) {
        cerr << "OSS: ftok failed: " << strerror(errno) << "\n";
        return 1;
    }

    g_shmid = shmget(key, sizeof(SimClock), 0666 | IPC_CREAT);
    if (g_shmid == -1) {
        cerr << "OSS: shmget failed: " << strerror(errno) << "\n";
        return 1;
    }

    g_clk = (SimClock*)shmat(g_shmid, nullptr, 0);
    if (g_clk == (SimClock*)-1) {
        cerr << "OSS: shmat failed: " << strerror(errno) << "\n";
        return 1;
    }

    g_clk->seconds     = 0;
    g_clk->nanoseconds = 0;

    // ── Init process table ────────────────────────────────────────────────────
    memset(g_table, 0, sizeof(g_table));

    // ── Tracking ──────────────────────────────────────────────────────────────
    int       launched      = 0;
    int       running       = 0;
    int       finished      = 0;
    long long totalRunNano  = 0;

    unsigned int lastLaunchS  = 0, lastLaunchNS  = 0;
    unsigned int lastPrintS   = 0, lastPrintNS   = 0;

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (launched < n || running > 0) {

        incrementClock(g_clk);

        // Print process table every 0.5 simulated seconds
        if (nanosBetween(lastPrintS, lastPrintNS,
                         g_clk->seconds, g_clk->nanoseconds) >= 500000000LL) {
            printProcessTable();
            lastPrintS  = g_clk->seconds;
            lastPrintNS = g_clk->nanoseconds;
        }

        // Non-blocking check for terminated children
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            for (int i = 0; i < TABLE_SIZE; i++) {
                if (g_table[i].occupied && g_table[i].pid == pid) {
                    totalRunNano += nanosBetween(
                        g_table[i].startSeconds, g_table[i].startNano,
                        g_clk->seconds, g_clk->nanoseconds
                    );
                    g_table[i].occupied = 0;
                    running--;
                    finished++;
                    cout << "OSS: Worker PID:" << pid
                         << " terminated at " << g_clk->seconds
                         << "s " << g_clk->nanoseconds << "ns\n";
                    break;
                }
            }
        }

        // Possibly launch a new child — respect -s and -i limits
        if (launched < n && running < s) {

            // Check interval: has enough simulated time passed since last launch?
            unsigned int checkS  = lastLaunchS + iSec;
            unsigned int checkNS = lastLaunchNS + iNano;
            if (checkNS >= 1000000000U) { checkS++; checkNS -= 1000000000U; }

            if (timeGTE(g_clk->seconds, g_clk->nanoseconds, checkS, checkNS)) {

                // Find free PCB slot
                int slot = -1;
                for (int i = 0; i < TABLE_SIZE; i++) {
                    if (!g_table[i].occupied) { slot = i; break; }
                }

                if (slot != -1) {
                    // Compute worker ending time
                    unsigned int endS  = g_clk->seconds + tSec;
                    unsigned int endNS = g_clk->nanoseconds + tNano;
                    if (endNS >= 1000000000U) { endS++; endNS -= 1000000000U; }

                    pid_t child = fork();
                    if (child == 0) {
                        // Child: exec worker with tSec and tNano as args
                        string secStr  = to_string(tSec);
                        string nanoStr = to_string(tNano);
                        execlp("./worker", "worker",
                               secStr.c_str(), nanoStr.c_str(), nullptr);
                        cerr << "OSS: execlp failed: " << strerror(errno) << "\n";
                        _exit(1);

                    } else if (child > 0) {
                        // Parent: fill PCB
                        g_table[slot].occupied          = 1;
                        g_table[slot].pid               = child;
                        g_table[slot].startSeconds      = g_clk->seconds;
                        g_table[slot].startNano         = g_clk->nanoseconds;
                        g_table[slot].endingTimeSeconds = endS;
                        g_table[slot].endingTimeNano    = endNS;

                        lastLaunchS  = g_clk->seconds;
                        lastLaunchNS = g_clk->nanoseconds;
                        launched++;
                        running++;

                        cout << "OSS: Launched worker PID:" << child
                             << " slot:" << slot
                             << " at " << g_clk->seconds
                             << "s " << g_clk->nanoseconds << "ns\n";
                    } else {
                        cerr << "OSS: fork failed: " << strerror(errno) << "\n";
                    }
                }
            }
        }
    }

    // ── Final report ──────────────────────────────────────────────────────────
    long long reportSec  = totalRunNano / 1000000000LL;
    long long reportNano = totalRunNano % 1000000000LL;

    cout << "\nOSS PID:" << getpid() << " Terminating\n"
         << finished << " workers were launched and terminated\n"
         << "Workers ran for a combined time of "
         << reportSec << " seconds " << reportNano << " nanoseconds.\n";

    // ── Cleanup ───────────────────────────────────────────────────────────────
    shmdt(g_clk);
    shmctl(g_shmid, IPC_RMID, nullptr);

    return 0;
}
