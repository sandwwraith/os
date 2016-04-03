#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void catcher(int signum, siginfo_t* siginfo, void* context) {
    printf("SIGUSR%d from %d\n", signum == (SIGUSR1) ? 1 : 2, siginfo->si_pid);
    exit(0);
}

int main() {
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    sigaddset(&action.sa_mask, SIGUSR2);
    action.sa_flags = SA_SIGINFO;

    action.sa_sigaction = &catcher;
    if (sigaction(SIGUSR1, &action, NULL) || sigaction(SIGUSR2, &action, NULL)) {
        perror("Cannot set sighandlers"); 
    }

    sleep(10);
    printf("No signals were caught\n");
}	
