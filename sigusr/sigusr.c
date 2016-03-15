#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

void catcher(int signum, siginfo_t* siginfo, void* context) {
    printf("Signal #%d from %d\n", signum , siginfo->si_pid);
    exit(0);
}

int main() {
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = &catcher;
	int i;
	for (i = 1; i<=31; ++i) {
		if (i == SIGKILL || i == SIGSTOP) continue;
		sigaddset(&action.sa_mask, i);
	}
	for (i = 1; i<=31; ++i) {
		if (i == SIGKILL || i == SIGSTOP) continue;
	
		if (sigaction(i, &action, NULL)) {
			perror("Cannot set sighandler"); 
		}
	}

	sleep(10);
	printf("No signals were caught\n");
	return 0;
}	
