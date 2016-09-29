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

#define LISTEN_MAX 4
#define FD_SIZE	200		//listen event_fd nums
#define EPOLL_EVENTS	100

static int sock_init(const char *ip, int port)
{
	int listenfd, ret;
	struct sockaddr_in svraddr;
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	CHECK_ERR_EXIT(listenfd, "socket error: ");

	memset(&svraddr, 0, sizeof(svraddr));
	svraddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, svraddr.sin_addr);
	svraddr.sin_port = port;

	ret = bind(listenfd, (struct sockaddr *)&svraddr, sizeof(struct sockaddr));
	CHECK_ERR_EXIT(ret,"bind error: ");

	listen(listenfd, LISTEN_MAX);
	
	return listenfd;
}

static int set_event(int epollfd, int fd, int state, int opt)
{
	struct epoll_event evt;

	evt.events = state;
	evt.data.fd = fd;
	
	epoll_ctl(epollfd, opt, fd, &evt);
	
	return 0;
}

static int do_read(int epollfd, int rfd, const char *buf)
{
	int nread;
	nread = read(rfd, buf, BUFF_MAX);
	if (nread == -1) {
		perror("read error: ");
		set_event(epollfd, rfd, EPOLLIN, EPOLL_CTL_DEL);
		return -1;
	} else if (nread == 0) {
		fprintf(stderr, "client already close !\n");
		set_event(epollfd, rfd, EPOLLIN, EPOLL_CTL_DEL);
	} else {
		printf("receive message: %s\n", buf);
		set_event(epollfd, rfd, EPOLLOUT, EPOLL_CTL_MOD);
	}
	
	return 0;
}

static int do_write(int epollfd, int wfd, char *buf)
{
	int nwrite;
	nwrite = write(wfd, buf, BUFF_MAX);
	if (nwrite == -1) {
		perror("write error: ");
		return -1;
	} else {
		if (fd == STDOUT_FILENO)
			set_event(epollfd, wfd, EPOLLOUT, EPOLL_CTL_DEL);
		else
			set_event(epollfd, wfd, EPOLLIN, EPOLL_CTL_MOD);
	}
	memset(buf, 0, BUFF_MAX);

	return 0;
}

static int do_accept(int epollfd, int listenfd)
{
	int clifd;
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	clifd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
	if (clifd == -1) {
		perror("accept");
		return -1;
	} else {
		printf("accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
		set_event(int epollfd, clifd, EPOLLIN, EPOLL_CTL_ADD);
	}

	return 0;
}


static int handle_events(int epollfd, struct epoll_event *events, int readyfds, int listenfd, char *buf)
{
	int i, fd;
	for (i = 0; i < readyfds; i++) {
		fd = events[i].data.fd;
		if (events[i].events & EPOLLIN) {
			if (fd == listenfd)	/* socket event, have new client request. */
				do_accept(epollfd, fd, buf);
			else
				do_read(epollfd, fd, buf);
		} else if (events[i].events & EPOLLOUT) {
			do_write(epollfd, fd, buf);
		}

		close(fd);
	}

	return 0;
}

static int do_epoll(int listenfd)
{
	int epollfd;
	struct epoll_event events[EPOLL_EVENTS];
	int readyfds;
	char buf[BUFF_MAX];

	epollfd = epoll_create(FD_SIZE);

	set_event(epollfd, listenfd, EPOLLIN, EPOLL_CTL_ADD);

	while(1) {
		/* get ready events num */
		readyfds = epoll_wait(epollfd, events, EPOLL_EVENTS, -1);
		handle_events();
	}

	close(epollfd);

	return 0;
}

int main()
{
	int listenfd;
	int port = 6666;
	char *ip_str = "127.0.0.1";

	listenfd = sock_init(ip_str, port);

	do_epoll(listenfd);
	close(listenfd);
	
	return 0;
}
