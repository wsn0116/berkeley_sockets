#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define HOSTNAME_MAX 255

struct require_values {
	int socket;
	char hostname[HOSTNAME_MAX];
	int port;
};

int connect_to_host (struct require_values *rv);
int socket_procedure (struct require_values *rv);
int send_ready (struct require_values *rv, struct timeval *tv);
void close_socket (struct require_values *rv);

int main (int argc, char *argv[])
{
	struct require_values rv;

	strcpy(rv.hostname, argv[1]);
	rv.port = atoi(argv[2]);

	if ((connect_to_host(&rv)) != 0) {
		perror("connect_to_host");
		return (-1);
	}

	fprintf(stdout, "Connected to %s:%d\n\n", rv.hostname, rv.port);

	if ((socket_procedure(&rv)) == -1) {
		close_socket(&rv);
		return (-1);
	}

	return 0;
}

int socket_procedure (struct require_values *rv)
{
	int nfds = -1;
	fd_set rfds;

	int stdin_fds = 0;

	int nrecv = 0;
	char *rbuffer, *wbuffer = 0;

	struct timeval tv;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(stdin_fds, &rfds);
		nfds = (nfds >= stdin_fds) ? nfds : stdin_fds;
		FD_SET(rv->socket, &rfds);
		nfds = (nfds >= rv->socket) ? nfds : rv->socket;
		if ((select(nfds + 1, &rfds, (fd_set *)0, (fd_set *)0, &tv)) != -1) {
			/* Receive packets from external socket */
			if (FD_ISSET(rv->socket, &rfds)) {
				ioctl(rv->socket, FIONREAD, &nrecv);
				switch (nrecv) {
					case -1:
						perror("ioctl");
						return (-1);
					case 0:
						close_socket(rv);
						return 0;
				}
				if ((rbuffer = calloc(nrecv, sizeof(char))) == NULL) {
					perror("calloc");
					return (-1);
				}
				if ((nrecv = recv(rv->socket, rbuffer, nrecv, 0)) == -1) {
					break;
				}
				rbuffer[nrecv] = '\0';
				fprintf(stdout, "recv : \n%s\n", rbuffer);
				free(rbuffer);
			}
			/* Receive packets from standard input */
			if (FD_ISSET(stdin_fds, &rfds)) {
				if ((ioctl(stdin_fds, FIONREAD, &nrecv)) == -1) {
					perror("ioctl");
					return (-1);
				}
				if ((wbuffer = calloc(nrecv + 1, sizeof(char))) == NULL) {
					perror("calloc");
					return (-1);
				}
				if ((nrecv = fread(wbuffer, sizeof(char), (size_t)nrecv, stdin)) == 0) {
					perror("fread");
					return (-1);
				}
				strcpy(&wbuffer[nrecv], "\n\0");

				while (!(send_ready(rv, &tv))) {}
				send(rv->socket, wbuffer, (strlen(wbuffer)), 0);
				fprintf(stdout, "send : %s\n", wbuffer);
				free(wbuffer);
			}
		}
	}

	return 0;
}

int connect_to_host (struct require_values *rv)
{
	struct sockaddr_in addr;
	struct hostent *info;

	if ( (rv->socket = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
		perror("socket");
		return (-1);
	}

	if( (info = gethostbyname(rv->hostname)) == NULL) {
		perror("gethostbyname");
		return (-1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(info->h_addr))->s_addr;
	addr.sin_port = htons(rv->port);

	if ((connect(rv->socket, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
		perror("connect");
		return (-1);
	}

	return 0;
}

int send_ready (struct require_values *rv, struct timeval *tv)
{
	fd_set wfds;

	FD_ZERO(&wfds);
	FD_SET(rv->socket, &wfds);

	if ((select(rv->socket + 1, (fd_set *)0, &wfds, (fd_set *)0, tv)) == -1) {
		perror("select");
		return 0;
	}

	if (FD_ISSET(rv->socket, &wfds)) {
		return 1;
	}

	return 0;
}

void close_socket (struct require_values *rv)
{
	close(rv->socket);
}

