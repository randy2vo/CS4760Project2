#include <iostream>
#include <string>
#include <unistd.h>     // getopt, fork, execvp
#include <sys/wait.h>   // waitpid
#include <cstdlib>      // exit, stoi
#include <cerrno>

using namespace std;

static void printHelp(const char* prog) {
    cout << "Usage: " << prog << " [-h] [-n proc] [-s simul] [-t iter]\n"
         << "  -h         Show this help message and exit\n"
         << "  -n proc    Total number of children to launch (max 80)\n"
         << "  -s simul   Max number of children allowed to run simultaneously (max 15)\n"
         << "  -t iter    Iterations to pass to ./user\n\n"
         << "Example:\n"
         << "  " << prog << " -n 5 -s 3 -t 7\n";
}

static bool parsePositiveInt(const char* s, int& out) {
    try {
        size_t pos = 0;
        int val = stoi(s, &pos);
        if (pos != string(s).size()) return false;
        if (val <= 0) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

static pid_t launchChild(int iter) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        string iterStr = to_string(iter);

        char* const args[] = {
            const_cast<char*>("./user"),
            const_cast<char*>(iterStr.c_str()),
            nullptr
        };

        execvp(args[0], args);

        // exec only returns on error
        perror("execvp");
        _exit(1);
    }

    return pid; // parent gets child's PID
}

int main(int argc, char* argv[]) {
    // Defaults
    int n = 1;  // total children
    int s = 1;  // simultaneous limit
    int t = 1;  // iterations for user

    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:")) != -1) {
        switch (opt) {
            case 'h':
                printHelp(argv[0]);
                return 0;

            case 'n':
                if (!parsePositiveInt(optarg, n) || n >= 80) {
                    cerr << "Error: -n requires a positive integer (less than 80)\n";
                    return 1;
                }
                break;

            case 's':
                if (!parsePositiveInt(optarg, s) || s >= 15) {
                    cerr << "Error: -s requires a positive integer (less than 15)\n";
                    return 1;
                }
                break;

            case 't':
                if (!parsePositiveInt(optarg, t)) {
                    cerr << "Error: -t requires a positive integer\n";
                    return 1;
                }
                break;

            default:
                printHelp(argv[0]);
                return 1;
        }
    }

    if (s > n) s = n;

    int running = 0;
    int totalLaunched = 0;

    // Launch initial batch
    while (running < s && totalLaunched < n) {
        pid_t childPid = launchChild(t);
        cout << "OSS PID:" << getpid()
             << " launched child PID:" << childPid << endl;
        running++;
        totalLaunched++;
    }

    // Launch remaining children
    while (totalLaunched < n) {
        int status;
        pid_t done = waitpid(-1, &status, 0);	// wait for any child to finish
        running--;	// free up one slot

        pid_t childPid = launchChild(t);
        cout << "OSS PID:" << getpid()
             << " child PID:" << done
             << " finished, launched child PID:" << childPid << endl;

        running++;
        totalLaunched++;
    }

    // Wait for remaining children
    while (running > 0) {
        wait(nullptr);
        running--;
    }

    cout << "OSS summary: launched " << totalLaunched
         << " total child processes\n";

  

    return 0;
}

