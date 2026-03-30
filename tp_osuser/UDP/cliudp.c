/*****
 * UDP client with acknowledgement reception
 *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define LBUF 512

static int create_socket(void)
{
    int sid;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sid < 0) {
        perror("socket");
        return -1;
    }
    return sid;
}

static int build_server_addr(struct sockaddr_in *sock, const char *host,
                             const char *port)
{
    struct hostent *h;

    h = gethostbyname(host);
    if (h == NULL) {
        herror(host);
        return -1;
    }

    bzero(sock, sizeof(*sock));
    sock->sin_family = AF_INET;
    bcopy(h->h_addr, &sock->sin_addr, h->h_length);
    sock->sin_port = htons((unsigned short)atoi(port));
    return 0;
}

static int wait_ack(int sid)
{
    int n;
    char buf[LBUF];
    socklen_t ls;
    struct sockaddr_in from;

    ls = sizeof(from);
    n = recvfrom(sid, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &ls);
    if (n == -1) {
        perror("recvfrom");
        return -1;
    }

    buf[n] = '\0';
    printf("AR recu : %s\n", buf);
    return 0;
}

int main(int argc, char *argv[])
{
    int sid;
    struct sockaddr_in sock;

    if (argc != 4) {
        fprintf(stderr, "Utilisation : %s nom_serveur port message\n", argv[0]);
        return 1;
    }

    sid = create_socket();
    if (sid < 0) {
        return 2;
    }

    if (build_server_addr(&sock, argv[1], argv[2]) != 0) {
        close(sid);
        return 3;
    }

    if (sendto(sid, argv[3], strlen(argv[3]), 0,
               (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        close(sid);
        return 4;
    }

    printf("Envoi OK !\n");

    if (wait_ack(sid) != 0) {
        close(sid);
        return 5;
    }

    close(sid);
    return 0;
}
