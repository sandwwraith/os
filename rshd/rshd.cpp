#include <iostream>
#include <string>

#include <sys/types.h> 
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <memory.h>
#include <sys/epoll.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 100

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
int make_epoll(Socket const& listen_sock) 
{
	int	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		close(epollfd);
		exit(EXIT_FAILURE);
	}
	return epollfd;
}

int main() 
{
	auto serv_sock = Socket(make_lstn_socket(2539));
	auto epoll = Epoll(make_epoll(serv_sock));
	epoll_event events[MAX_EVENTS];
	for (int num_events;;) // Main cycle
	{
		num_events = epoll_wait(epoll, events, MAX_EVENTS, -1);
		if (num_events == -1) {
			perror("Wait error");
			exit(EXIT_FAILURE);
		}
		for (int n = 0; n < num_events; ++n) 
		{
			if (events[n].data.fd == serv_sock) 
			{
				//Incoming connection
				sockaddr_in cli_addr;
				memset(&cli_addr,0,sizeof(sockaddr_in));
				socklen_t len = sizeof(sockaddr_in);
				auto cli_fd = accept(serv_sock, (struct sockaddr*) &cli_addr, &len);
				std::cout<<"Client accepted!\n";			

				epoll_event ev;
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = cli_fd;
				if (epoll_ctl(epoll, EPOLL_CTL_ADD, cli_fd, &ev) == -1) {
					perror("epoll_ctl: client");
					exit(EXIT_FAILURE);
				}
			}
			else 
			{
				//Working with client
				auto fd = events[n].data.fd;
				char buf[BUFFER_SIZE];
				auto r = read(fd, buf, BUFFER_SIZE);
				if (r <= 0) {
					close(fd);
					continue;
				}
				std::string str(buf, r);
				std::cout<<"Client send: " << str;
			}
		}
	}

	return 0;
}
