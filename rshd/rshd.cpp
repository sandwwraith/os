#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <deque>

#include <sys/types.h> 
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <memory.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <wait.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

/**
 * Wrapper RAII class
 */
class RAIIFD 
{
	private:
		int fd;
	public:
		RAIIFD(int socket) : fd(socket) {};
		~RAIIFD() 
		{
			close(fd);
		}
		operator int const&() const 
		{
			return fd;
		}
		RAIIFD(RAIIFD const& other) = delete;
		RAIIFD(RAIIFD&& other) 
		{
			this->fd = other.fd;
		}
};

using Socket = RAIIFD;
using Epoll = RAIIFD;

enum context_t
{
	server, client, pty
};

struct context
{
	context_t type;
	RAIIFD fd;
	context* pair;
	pid_t child_proc = -1;
	char rbuf[BUFFER_SIZE];
	std::deque<std::string> write_buf;
	context(context_t t, int fd) : type(t), fd(fd)
	{
		memset(rbuf, 0, BUFFER_SIZE);
	}
	int contread()
	{
		std::string str;
		ssize_t r = 1;
		while (r > 0) {
			r = read(fd, rbuf, BUFFER_SIZE);
			if (r>0)
			str+=std::string(rbuf,r);
		}
		if (r < 0 && errno!= EAGAIN) {
			//error
			return -1;
		}
		if ( str.size() == 0) return -1; // 0 read bytes mean EOF
		std::cout<< (this->type == context_t::client ? "Client" : "PTY" )<< " send: " << str;
		pair->write_buf.push_back(str);
		pair->contwrite();
		return 0;
	}
	int contwrite()
	{
		while (!write_buf.empty())
		{
			std::string str = write_buf.front();
			write_buf.pop_front();
			std::cout << "Writing " << str << std::endl;
			const char* buf = str.c_str();
			size_t r = str.size();
			ssize_t wr = r;
			ssize_t res;

			while (wr > 0 && (res = write(fd, buf+r-wr, wr))) {
			//	std::cout <<"wr "<< wr << " res " << res <<std::endl;
				if (res == -1) break;
				wr = r - res;
			}
			if (wr > 0) {
				if (errno != EAGAIN) return -1;
				std::string left(buf+r-wr,wr);
				write_buf.push_front(left);
				break;
			}
		}
		std::cout<<"Write complete\n";
		return 0;
	}
	context (context const& other) = delete;
};
/**
 * Creates listen server socket, binds it to given port and starts listening.
 */
int make_lstn_socket(int port) 
{
	struct sockaddr_in serv_addr;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("ERROR opening socket");
		exit(EXIT_FAILURE);
	}
	memset(&serv_addr, 0, sizeof(sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) 
	{
		perror("ERROR on binding");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	listen(sockfd, 42);
	return sockfd;
}

/**
 * Creates a epoll FD for listening server socket
 */
int make_epoll(context* listen_sock) 
{
	int	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = (void*)listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock->fd, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		close(epollfd);
		exit(EXIT_FAILURE);
	}
	return epollfd;
}

int accept_conn(int serv_sock)
{
	sockaddr_in cli_addr;
	memset(&cli_addr,0,sizeof(sockaddr_in));
	socklen_t len = sizeof(sockaddr_in);
	auto cli_fd = accept(serv_sock, (struct sockaddr*) &cli_addr, &len);
	std::cout<<"Client accepted!\n";			
	return cli_fd;
}

void add_to_epoll(Epoll const& epoll, context* client)
{
	epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLET;
	ev.data.ptr = (void*) client;
	if (epoll_ctl(epoll, EPOLL_CTL_ADD, client->fd, &ev) == -1) {
		perror("epoll_ctl: client");
		exit(EXIT_FAILURE);
	}
}

int create_master_pty()
{
	int fdm = posix_openpt(O_RDWR);
	if (fdm < 0)
	{
		perror("Open pty err");
		exit(EXIT_FAILURE);
	}
	if (grantpt(fdm) || unlockpt(fdm)) {
		perror("Unlocking pty err");
		exit(EXIT_FAILURE);
	}
	return fdm;
}

void make_nonblocking(int fd)
{
	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		perror("Non bocking set failed");
	}
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		perror("Non bocking set failed");
	}
}

std::string const daemon_file = "/tmp/rshd.pid";
std::string const daemon_err_log = "/tmp/rshd.err.log";

void demonize()
{
	std::ifstream inf(daemon_file);
	if (inf)
	{
		int pid;
		inf >> pid;
		if (!kill(pid, 0))
		{
			std::cerr << "Daemon is already running with PID " << pid<<std::endl;
			exit(pid);
		}
	}
	inf.close();
	//start forking
	auto res = fork();
	if (res == -1) {
		perror("Fork FAIL");
		exit(EXIT_FAILURE);
	}

	if (res != 0)
	{
		//Parent not needed anymore
		exit(EXIT_SUCCESS);
	}
	//Child here
	setsid();

	int daemon_pid = fork();

	if (daemon_pid)
	{
		std::ofstream ouf(daemon_file, std::ofstream::trunc);
		ouf<<daemon_pid;
		ouf.close();
		exit(EXIT_SUCCESS);
	}
	//Redirecting STD streams to /dev/null
	int slave = open("/dev/null", O_RDWR);
	int err = open(daemon_err_log.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	dup2(slave, STDIN_FILENO);
	dup2(slave, STDOUT_FILENO);
	dup2(err, STDERR_FILENO);
	close(slave);
	close(err);
	//Running main function from daemon
	return;
}

std::vector<std::shared_ptr<context>> clients;
std::vector<std::shared_ptr<context>> terms;

static volatile bool exiting = false;

void catcher(int signum, siginfo_t* siginfo, void* context) {
	exiting = true;
}

void finalize() {
	for (auto ptr:clients)
	{
		pid_t child_proc = ptr->child_proc;

		std::cerr<<"Killing "<<child_proc<<std::endl;
		int status;
		kill(child_proc, SIGKILL);
		waitpid(child_proc, &status, 0);
	}
	remove(daemon_file.c_str());
	exit(0);
}

int main(int argc, char* argv[])
{
	int port = 2539;
	if (argc < 2)
	{
		fprintf(stderr,"ERROR, no port provided\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		port = atoi(argv[1]);
	}
	demonize();

	struct sigaction action;
	action.sa_flags = SA_SIGINFO;

	action.sa_sigaction = &catcher;
	if (sigaction(SIGTERM, &action, NULL) || sigaction(SIGINT, &action, NULL)) {
		perror("Cannot set sighandlers");
		exit(EXIT_FAILURE);
	}

	auto serv_sock = std::shared_ptr<context>(new context(context_t::server,make_lstn_socket(port)));
	auto epoll = Epoll(make_epoll(serv_sock.get()));
	epoll_event events[MAX_EVENTS];
	for (int num_events;;) // Main cycle
	{
		num_events = epoll_wait(epoll, events, MAX_EVENTS, -1);
		if (exiting) break;
		if (num_events == -1) {
			perror("Wait error");
			exit(EXIT_FAILURE);
		}
		for (int n = 0; n < num_events; ++n) 
		{
			context* cont = (context*) events[n].data.ptr;
			if (cont->type == context_t::server) 
			{
				//Incoming connection
				std::shared_ptr<context> client = std::make_shared<context>(context_t::client, accept_conn(serv_sock->fd));
				std::shared_ptr<context> terminal = std::make_shared<context>(context_t::pty, create_master_pty());

				client->pair = terminal.get();
				terminal->pair = client.get();

				clients.push_back(client);
				terms.push_back(terminal);

				make_nonblocking(client->fd);
				make_nonblocking(terminal->fd);

				add_to_epoll(epoll, terminal.get());
				add_to_epoll(epoll, client.get());

				int slave = open(ptsname(terminal->fd), O_RDWR);
				//Launching shell
				auto proc= fork();
				if (!proc)
				{
					//Destructors will clean up all file descriptors
					clients.clear();
					terms.clear();
					client.reset();
					terminal.reset();
					serv_sock.reset();
					close(epoll);

					//Setting new terminal
					struct termios slave_orig_term_settings; // Saved terminal settings
					struct termios new_term_settings; // Current terminal settings
					tcgetattr(slave, &slave_orig_term_settings);
					new_term_settings = slave_orig_term_settings;
					new_term_settings.c_lflag &= ~(ECHO | ECHONL | ICANON);
					//cfmakeraw (&new_term_settings);
					tcsetattr (slave, TCSANOW, &new_term_settings);

					//Setting new terminal as STD
					dup2(slave, STDIN_FILENO);
					dup2(slave, STDOUT_FILENO);
					dup2(slave, STDERR_FILENO);
					close(slave);

					// Make the current process a new session leader
					setsid();

					// As the child is a session leader, set the controlling terminal to be the slave side of the PTY
					// (Mandatory for programs like the shell to make them manage correctly their outputs)
					ioctl(0, TIOCSCTTY, 1);

					execlp("/bin/sh","sh", NULL);
				}
				else
				{
					close(slave);
					client->child_proc = proc;
				}
			}
			else 
			{
				//Working with client
				int res = -1;
				if ((events[n].events & EPOLLIN) != 0) {
					std::cout<<"Event READ\n";
					res = cont->contread();
				} else if ((events[n].events & EPOLLOUT) != 0){
					std::cout<<"Event WRITE\n";
					res = cont->contwrite();
				}
				if (res == -1)
				{
					std::cerr<<"Disconnecting client"<<std::endl;
					context* cterm = cont->pair;
					if (cterm->type == context_t::client) std::swap(cterm, cont);
					for (auto it = clients.begin(); it!=clients.end(); ++it) {
						if (it->get() == cont) {
							epoll_ctl(epoll, EPOLL_CTL_DEL, it->get()->fd, events+n);
							clients.erase(it);
							break;
						}
					}
					for (auto it = terms.begin(); it!=terms.end(); ++it) {
						if (it->get() == cterm) {
							epoll_ctl(epoll, EPOLL_CTL_DEL, it->get()->fd, events+n);
							terms.erase(it);
							break;
						}
					}
				}
			}
		}
	}

	finalize();
	return EXIT_SUCCESS;
}
