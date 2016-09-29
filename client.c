#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>       	/* basic system data types */
#include <sys/socket.h>      	/* basic socket definitions */
#include <netinet/in.h>      	/* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>       	/* inet(3) functions */
#include <sys/epoll.h> 			/* epoll function */
#include <fcntl.h>     			/* nonblocking */
#include <sys/resource.h> 		/*setrlimit */

#include "trans_protocol.h"

#define FDSIZE	128
#define EPOLLEVENTS 10

static int sock_init(const char *ip_str, int port)
{
	int sockfd, ret;
	struct sockaddr_in svraddr;

	memset(&svraddr, 0, sizeof(svraddr));
	svraddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip_str, svraddr.sin_addr);
	svraddr.sin_port = port;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	CHECK_ERR_EXIT(sockfd, "socket error: ");

	ret = connect(sockfd, (struct sockaddr *)&svraddr, sizeof(struct sockaddr));

	return sockfd;
}

static int set_event(int epollfd, int fd, int state, int opt)
{
	struct epoll_event evt;

	evt.events = state;
	evt.data.fd = fd;
	
	epoll_ctl(epollfd, opt, fd, &evt);
	
	return 0;
}

static int do_read(int epollfd, int rfd, int sockfd, const char *buf)
{
	int nread;
	
	nread = read(rfd, buf, BUFF_MAX);
	if (nread == -1) {
		perror("read error: ");
		return -1;
	} else if (nread == 0) {
		fprintf(stderr, "server close\n");
		return 0;
	} else {
		if (rfd == STDIN_FILENO) {
			/* socket write to server */
			set_event(epollfd, sockfd, EPOLLOUT, EPOLL_CTL_ADD);
		} else {	
			/* delete socket event, not read msg from server */
			set_event(epollfd, sockfd, EPOLLIN, EPOLL_CTL_DEL);
			/* add write event */
			set_event(epollfd, STDOUT_FILENO, EPOLLOUT, EPOLL_CTL_ADD);
		}
	}
	
	return 0;
}

static int do_write(int epollfd, int wfd, int sockfd, char *buf)
{
	int nwrite;

	nwrite = write(wfd, buf, sizeof(buf));
	if (nwrite == -1) {
		perror("write error: ");
		return -1;
	} else {
		if (wfd == STDOUT_FILENO)
			set_event(epollfd, wfd, EPOLLOUT, EPOLL_CTL_DEL);
		else
			/* change to wait read events */
			set_event(epollfd, wfd, EPOLLIN, EPOLL_CTL_MOD);
	}
	memset(buf, 0, BUFF_MAX);

	return 0;
}

static int handle_events(int epollfd, struct epoll_event events, int readyfds, int sockfd, char *buf)
{
	int fd;
	int i;

	for (i = 0; i < readyfds; i++) {
		fd = events[i].data.fd;
		
		if (events[i].events & EPOLLIN)
			do_read(epollfd, fd, sockfd, buf);
		else if (events[i].events & EPOLLOUT)
			do_write(epollfd, fd, sockfd, buf);

		close(fd);
	}

	return 0;
}

static int do_epoll(int sockfd)
{
	int epollfd;
	struct epoll_event events[EPOLLEVENTS];
	int readyfds;
	char buf[BUFF_MAX];
	
	memset(buf, 0, BUFF_MAX);
	epollfd = epollfd_create(FDSIZE);
	set_event(epollfd, STDIN_FILENO, EPOLLIN, EPOLL_CTL_ADD);

	while(1) {
		readyfds = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
		handle_events(epollfd, &events, readyfds, sockfd, buf);

	}

	close(epollfd);

	return 0;
}

int main(int argc, char *argv[])
{
	char *ip = "127.0.0.1";
	int port = 8888;

	int sockfd;

	sockfd = sock_init(ip, port);

	do_epoll(sockfd);
	close(sockfd);

	return 0;
}
