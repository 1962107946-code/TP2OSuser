/*****
 * UDP server with acknowledgement
 *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9999
#define LBUF 512

static char buf[LBUF + 1];
static struct sockaddr_in SockConf;

static char *addrip(unsigned long addr)
{
    static char bufip[16];

    snprintf(bufip, sizeof(bufip), "%u.%u.%u.%u",
             (unsigned int)((addr >> 24) & 0xFF),
             (unsigned int)((addr >> 16) & 0xFF),
             (unsigned int)((addr >> 8) & 0xFF),
             (unsigned int)(addr & 0xFF));
    return bufip;
}

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

static int bind_socket(int sid)
{
    bzero(&SockConf, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_port = htons(PORT);
    SockConf.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sid, (struct sockaddr *)&SockConf, sizeof(SockConf)) == -1) {
        perror("bind");
        return -1;
    }
    return 0;
}

static void handle_message(int sid)
{
    int n;
    char ack[] = "Bien recu 5/5 !";
    socklen_t ls;
    struct sockaddr_in Sock;

    ls = sizeof(Sock);
    n = recvfrom(sid, (void *)buf, LBUF, 0, (struct sockaddr *)&Sock, &ls);
    if (n == -1) {
        perror("recvfrom");
        return;
    }

    buf[n] = '\0';
    printf("recu de %s : <%s>\n", addrip(ntohl(Sock.sin_addr.s_addr)), buf);

    if (sendto(sid, ack, strlen(ack), MSG_CONFIRM,
               (struct sockaddr *)&Sock, ls) == -1) {
        perror("sendto ack");
    }
}

int main(void)
{
    int sid;

    sid = create_socket();
    if (sid < 0) {
        return 2;
    }

    if (bind_socket(sid) != 0) {
        return 3;
    }

    printf("Le serveur est attache au port %d !\n", PORT);
    for (;;) {
        handle_message(sid);
    }

    return 0;
}
