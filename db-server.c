#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "db.h"

#define PORT "11211"

typedef socklen_t               sl_t;
typedef struct sockaddr         sa_t;
typedef struct sockaddr_in      si_t;
typedef struct sockaddr_in6     s6_t;
typedef struct sockaddr_storage ss_t;

typedef struct sbuf {
	int   max;
	int   off;
	int   len;
	char *buf;
} sbuf_t;

typedef struct conn {
	sbuf_t in;
	sbuf_t out;
} conn_t;

conn_t conn[FD_SETSIZE];	/* FD_SETSIZE not a big number anyway */

#define INBUF_LEN		(1 << 26)	/* 64MB for read request  */
#define OUTBUF_LEN		(1 << 26)	/* 64MB for send response */

#define VALBUF_LEN		(1 << 26)	/* 64MB for read database */

char *inbuf;
char *outbuf;

char *valbuf;

enum {HANDLE_CLOSE, HANDLE_FINISH, HANDLE_NEEDMOREIN, HANDLE_NEEDMOREOUT};

#define ARGC_MAX	6
#define ARGV_MAX	1024			/* also max key size */

ssize_t
sbuf_send(const int fd, sbuf_t *buf, ssize_t *snd)
{
	ssize_t len;

	assert(buf->len <= buf->max);
	assert(buf->off <  buf->len);

	*snd = 0;
	do {
		len = send(fd, buf->buf + buf->off, buf->len - buf->off, 0);
		if (len > 0) {
			*snd     += len;
			buf->off += len;
		}
	} while (buf->off > 0 && buf->off < buf->len && len > 0);

	if (len == -1 && errno == EWOULDBLOCK)
		len = 1;	/* don't error when recv block */

	return len;	/* err */
}

ssize_t
sbuf_recv(const int fd, sbuf_t *buf, ssize_t *rcv)
{
	ssize_t len;

	assert(buf->len < buf->max);

	*rcv = 0;
	do {
		len = recv(fd, buf->buf + buf->len, buf->max - buf->len, 0);
		if (len > 0) {
			*rcv     += len;
			buf->len += len;
		}
	} while (buf->len > 0 && buf->len < buf->max && len > 0);

	if (len == -1 && errno == EWOULDBLOCK)
		len = 1;	/* don't error when recv block */

	return len;	/* err */
}

void
sbuf_allocate(sbuf_t *buf, char *data, const ssize_t size)
{
	buf->max = size;
	buf->len = 0;
	buf->off = 0;
	buf->buf = data;
}

void
sbuf_release(sbuf_t *buf)
{
	buf->max = 0;
	buf->len = 0;
	buf->off = 0;
	buf->buf = NULL;
}

int
keylen(const char *key, const int maxlen)
{
	char *p = (char *)memchr(key, ' ', maxlen);
	if (p == NULL)
		p = (char *)memchr(key, '\r', maxlen);
	if (p == NULL)
		return 0;
	return p - key;
}

int
argparse(const char *buf, const ssize_t buflen,
	int *argc, char argv[][ARGV_MAX], const int max)
{
	int i;

	int p;
	int c;

	int arg;
	int len;

	assert(buflen > 0);

	p   = 0;
	arg = 0;
	len = 0;
	for (i = 0; i < buflen; i++) {
		c = buf[i];

		if (len + 1 > ARGV_MAX) 	/* 1 for '\0' */
			goto err;

		switch (c) {
		case '\n':
			if (p != '\r')
				goto err;
			goto out;
		case '\r':
		case ' ':
			memcpy(argv[arg], buf + i - len, len);
			argv[arg][len] = '\0';
			if (arg == max)
				goto out;

			len  = 0;
			arg += 1;;
			break;
		default:
			len++;
		}

		p = c;
	}

	return 0;
out:
	*argc = arg;
	return (i + 1);
err:
	return -1;
}

int
handle(const int fd, db_t *db, sbuf_t *in, sbuf_t *out)
{
	ssize_t err;
	ssize_t len;

	char *key;
        uint32_t klen;
	uint32_t vlen;

	int  argc;
	int  arglen;
	char argv[ARGC_MAX][ARGV_MAX];

	if ((err = sbuf_recv(fd, in, &len)) <= 0 && len == 0) {
		if (err == -1) {
			fprintf(stdout, "db-server: socket %d recv %s\n", fd, strerror(errno));
		}
		return HANDLE_CLOSE;
	}

	arglen = argparse(in->buf, in->len, &argc, argv, ARGC_MAX);
	if (argc < 1 || arglen == -1) {
		fprintf(stderr, "db-server: socket %d malformed request\n", fd);

		return HANDLE_CLOSE;
	}

	if (strcmp(argv[0], "set") == 0 && argc >= 5) {
		key  = argv[1];
		klen = strlen(key);

		vlen = atoi(argv[4]);
		if (vlen + arglen > in->max) {
			fprintf(stderr, "db-server: socket %d too large value\n", fd);

			return HANDLE_CLOSE;
		}

		if (vlen + arglen > in->len) {
			return HANDLE_NEEDMOREIN;
		}

		if (db_put(db, key, klen, in->buf + arglen, vlen) == DB_OK) {
			out->len = sprintf(out->buf, "STORED\r\n");
		} else {
			out->len = sprintf(out->buf, "ERROR\r\n");
		}
	} else if (strcmp(argv[0], "get") == 0 && argc >= 2) {
		key  = argv[1];
		klen = strlen(key);

		if ((vlen = db_get(db, key, klen, valbuf, VALBUF_LEN)) != 0) {
			if (vlen > VALBUF_LEN)
				return HANDLE_CLOSE;

			out->len = snprintf(out->buf, out->max,
			                  "VALUE %.*s %d %d\r\n%.*s\r\nEND\r\n",
			                  klen, key, 0, vlen, vlen, valbuf);
		} else {
			out->len = sprintf(out->buf, "END\r\n");
		}
	} else if (strcmp(argv[0], "delete") == 0 && argc >= 2) {
		key  = argv[1];
		klen = strlen(key);

		if ((db_del(db, key, klen)) != 0) {
			out->len = snprintf(out->buf, out->max, "DELETED\r\n");
		} else {
			out->len = snprintf(out->buf, out->max, "NOT_FOUND\r\n");
                }
        } else {
		return HANDLE_CLOSE;
	}

	if ((err = sbuf_send(fd, out, &len)) <= 0 && len == 0) {
		if (err == -1) {
			fprintf(stdout, "db-server: socket %d send %s\n", fd, strerror(errno));
		}
		return HANDLE_CLOSE;
	}

	if (out->off < out->len) {
		return HANDLE_NEEDMOREOUT;
	}

	return HANDLE_FINISH;
}

int
handle_write(const int fd, fd_set *readfds, fd_set *writefds)
{
	sbuf_t *in;
	sbuf_t *out;

	ssize_t err;
	ssize_t len;

	in  = &conn[fd].in;
	out = &conn[fd].out;

	if (out->buf == NULL || out->len == 0 || out->off >= out->len) {
		FD_CLR(fd, writefds);

		return 0;
	}

	if ((err = sbuf_send(fd, out, &len)) <= 0 && len == 0) {
		FD_CLR(fd, writefds);

		return 0;
	}
	if (out->off == out->len) {
		inbuf  = in->buf;
		outbuf = out->buf;

		sbuf_release(in);
		sbuf_release(out);
	}
	return 0;
}

int
handle_read(const int fd, db_t *db, fd_set *readfds, fd_set *writefds)
{
	sbuf_t *in;
	sbuf_t *out;

	ssize_t err;

	in  = &conn[fd].in;
	out = &conn[fd].out;

	if (in->buf == NULL) {
		if (inbuf == NULL) {
			inbuf = malloc(INBUF_LEN);
		}
		sbuf_allocate(in, inbuf, INBUF_LEN);

		inbuf = NULL;
	}
	if (out->buf == NULL) {
		if (outbuf == NULL) {
			outbuf = malloc(OUTBUF_LEN);
		}
		sbuf_allocate(out, outbuf, OUTBUF_LEN);

		outbuf = NULL;
	}

	err = handle(fd, db, in, out);

	if (err == HANDLE_FINISH) {
		if (inbuf == NULL) {
			inbuf   = in->buf;
			in->buf = NULL;
		}
		if (outbuf == NULL) {
			outbuf   = out->buf;
			out->buf = NULL;
		}

		free(in->buf);
		free(out->buf);

		sbuf_release(in);
		sbuf_release(out);

		return 0;
	}

	if (err == HANDLE_NEEDMOREIN) {
		return 0; /* do nothing */
	}

	if (err == HANDLE_NEEDMOREOUT) {
		FD_SET(fd, writefds);	/* writer buf full need write event */

		return 0; /* do nothing */
	}

	if (err == HANDLE_CLOSE) {
		fprintf(stdout, "db-server: socket %d close\n", fd);

		if (inbuf == NULL) {
			inbuf   = in->buf;
			in->buf = NULL;
		}
		if (outbuf == NULL) {
			outbuf   = out->buf;
			out->buf = NULL;
		}

		free(in->buf);
		free(out->buf);

		sbuf_release(in);
		sbuf_release(out);

		close(fd);
		FD_CLR(fd, readfds);
		FD_CLR(fd, writefds);

		return -1;
	}

	return 0;
}

int
handle_accept(const int fd, fd_set *readfds, fd_set *writefds)
{
	ss_t addr;
	sl_t addrlen;
	char addrstr[INET6_ADDRSTRLEN];
	int  acceptfd;

	void *in_addr;

	addrlen  = sizeof(addr);
	acceptfd = accept(fd, (sa_t *)&addr, &addrlen);

	if (acceptfd == -1) {
		fprintf(stdout, "db-server: accept: %s\n", strerror(errno));

		return -1;
	}

	if (acceptfd >= FD_SETSIZE) {
		fprintf(stdout, "db-server: socket %d closed,can't take more fd\n", fd);

		close(acceptfd);

		return -1;
	}


	if (fcntl(acceptfd, F_SETFL, O_NONBLOCK) == -1) {
		fprintf(stdout, "db-server: socket %d fcntl NONBLOCK: %s\n", fd, strerror(errno));

		close(acceptfd);

		return -1;
	}

	if (addr.ss_family == AF_INET) {
		in_addr = &((si_t *)&addr)->sin_addr;
	} else if (addr.ss_family == AF_INET6) {
		in_addr = &((s6_t *)&addr)->sin6_addr;
	} else {
		in_addr = NULL;
	}
	if (inet_ntop(addr.ss_family, in_addr, addrstr, sizeof(addrstr)) == NULL) {
		fprintf(stdout, "db-server: socket %d unknown address family\n", fd);
	}
	printf("db-server: new connection from %s on socket %d\n", addrstr, acceptfd);

	FD_SET(acceptfd, readfds);

	return acceptfd;
}

int
main(int argc, char *argv[])
{
	int err;
	int nfds;
	int socketfd;

        db_t db;
        db_option_t option;

	fd_set readfds;
	fd_set writefds;

	char *dbfilename;
	char *idxfilename;

	struct addrinfo hints, *ai, *p;

	if (argc == 2) {
		dbfilename  = argv[1];
		idxfilename = NULL;
	} else if (argc != 3) {
		dbfilename  = argv[1];
		idxfilename = argv[2];
	} else {
		fprintf(stderr, "usage: %s dbfile [indexfile]\n", argv[0]);

		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;

	if ((err = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "db-server: getaddrinfo: %s\n", gai_strerror(err));

		exit(1);
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		int optval = 1;

		socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socketfd == -1) {
			fprintf(stderr, "db-server: socket: %s\n", gai_strerror(err));

			continue;
		}
		if (fcntl(socketfd, F_SETFL, O_NONBLOCK) == -1) {
			fprintf(stderr, "db-server: fcntl NONBLOCK: %s\n", strerror(errno));

			continue;
		}

		if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
			fprintf(stderr, "db-server: setsockopt REUSEADDR: %s\n", strerror(errno));

			continue;
		}

		if (bind(socketfd, p->ai_addr, p->ai_addrlen) < 0) {
			fprintf(stderr, "db-server: bind: %s\n", strerror(errno));

			close(socketfd);

			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "db-server: failed to bind: %s\n", PORT);
		exit(1);
	}

	freeaddrinfo(ai);

        option.table  = 256;
        option.bucket = 256;
        option.rdonly = 0;
        if (db_open(&db, dbfilename, idxfilename, &option) != DB_OK) {
                fprintf(stderr, "db-server: open db %s failed\n", dbfilename);

                exit(0);
        }

	if (listen(socketfd, 32) == -1) {
		fprintf(stderr, "db-server: listen: %s\n", strerror(errno));

		exit(1);
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	nfds = socketfd;
	FD_SET(socketfd, &readfds);

	inbuf  = malloc(INBUF_LEN);
	outbuf = malloc(OUTBUF_LEN);
	valbuf = malloc(VALBUF_LEN);

	for (;;) {
		int n;
		int fd;
		fd_set readfds_;
		fd_set writefds_;

		FD_ZERO(&readfds_);
		FD_ZERO(&writefds_);

#ifdef LINUX
		memcpy(&readfds_, &readfds, sizeof(readfds));
		memcpy(&writefds_, &writefds, sizeof(writefds));
#else
		FD_COPY(&readfds, &readfds_);
		FD_COPY(&writefds, &writefds_);
#endif

		if ((n = select(nfds + 1, &readfds_, &writefds_, NULL, NULL)) == -1) {
			fprintf(stderr, "db-server: select: %s\n", strerror(errno));

			exit(1);
		}

		for (fd = 0; fd <= nfds && n > 0; fd++) {
			if (FD_ISSET(fd, &writefds_)) {
				n--;
				handle_write(fd, &readfds, &writefds);
			}

			if (FD_ISSET(fd, &readfds_)) {
				n--;
				if (fd == socketfd) {
					int acceptfd;

					acceptfd = handle_accept(fd, &readfds, &writefds);
					if (acceptfd > nfds) {
						nfds = acceptfd;
					}
				} else {
					err = handle_read(fd, &db, &readfds, &writefds);
					if (err == -1 && fd >= nfds) {
						nfds = fd;
						while (FD_ISSET(nfds, &readfds)  == 0 &&
						       FD_ISSET(nfds, &writefds) == 0)
						{
							nfds--;
						}
					}
				}
			}
		}
	}
	free(inbuf);
	free(outbuf);
	free(valbuf);

	return 0;
}
