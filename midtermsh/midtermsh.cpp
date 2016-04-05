#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define BUF_LEN 100

pid_t cur_child = -1;
int pip_fd = -1;
std::vector<pid_t> pids;

void handler(int signum, siginfo_t* siginfo, void* context) {
	for (pid_t& pid : pids) {
		kill(pid, signum);
	}
}
using strings = std::vector<std::string>;

pid_t launch_all(std::string command, int stdIn, int stdOut) {
	std::vector<int*> pipes;
//	std::cerr<<"Command:"<<command<<std::endl;
	strings coms;
	size_t f = 0,pf = 0;
	command.push_back(' ');
	while (f<command.size()) {
		f = command.find("|", pf);
		if (f == std::string::npos) f = command.size();
		coms.push_back(command.substr(pf, f-pf-1));
		pf = f+2;
	}

	int pipel[2];
	int piper[2];

	for (size_t i = 0; i< coms.size(); i++) {
//		std::cerr<<"Program:"<<coms[i]<<std::endl;
		pipe(piper);
	
		pid_t fo = fork();
		if (fo) {
			if (i) {
				close(pipel[0]);
				close(pipel[1]);
			}
			if (i == coms.size() - 1) {
				close(piper[0]);
				close(piper[1]);
			}
		} else {
			if (i) {
				dup2(pipel[0], STDIN_FILENO);
				close(pipel[0]);
				close(pipel[1]);
			}
			if (i != coms.size() -1 ) {
				dup2(piper[1], STDOUT_FILENO);
			}
			close(piper[0]);
			close(piper[1]);

			std::vector<char* > vec;
			char* fname;
			char comc[coms[i].size()+1];
			strcpy(comc, coms[i].c_str());
			char* pch = fname = strtok(comc, " ");
			while (pch!=NULL)
			{	
				vec.push_back(pch);
				pch = strtok(NULL, " ");
			}
			vec.push_back(NULL);
			execvp(fname, vec.data());
		}
		pids.push_back(fo);
		pipel[0] = piper[0];
		pipel[1] = piper[1];
	}

	pid_t cp;
	if ((cp = fork())) {
		return cp;
	} else {
		//waiting all childs
		for (pid_t& pid : pids) {
			int status;
			waitpid(pid, &status, 0); 
		}
		exit(0);
	}
}


std::pair<pid_t, int > create_childs(char* buffer, int len) {
	size_t pos;
	for (pos  = 0; (int)pos < len; ++pos) {
		if (buffer[pos] == '\n') {
			break;	
		}
	}
	int pipefd[2];
	pipe(pipefd);
	//std::cerr<<"$$$"<<std::string(buffer,0,pos)<<std::endl;
	pid_t ch = launch_all(std::string(buffer, 0, pos), pipefd[0], STDOUT_FILENO);
	close(pipefd[0]);
	write(pipefd[1], buffer+pos, len-pos);
	return std::make_pair(ch, pipefd[1]);
}

int main() {
	struct sigaction act;
	act.sa_sigaction = &handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &act, NULL);

	char buf[BUF_LEN];
	int r = 1;
	int status;
	while (1){
		r = read(0, &buf, BUF_LEN);
		if (r <= 0) { 
			waitpid(cur_child, &r, 0);
			exit(0);
		}
		if (cur_child != -1 && waitpid(cur_child, &status, WNOHANG) == 0) {
			//No state has changed, children running
			write(pip_fd, buf, r); 
		} else {
			//Child exited or didn't exist
			pids.clear();
			close(pip_fd); //close(-1) is error, but OKAY
			auto p = create_childs(buf, r);
			cur_child = p.first;
			pip_fd = p.second;
		}
	}
	return 0;
}
