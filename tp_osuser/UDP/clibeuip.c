#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9998
#define LBUF 512
#define MAGIC "BEUIP"
#define MAGIC_LEN 5

static void print_usage(const char *prog)
{
    fprintf(stderr, "Utilisation :\n");
    fprintf(stderr, "  %s liste\n", prog);
    fprintf(stderr, "  %s send pseudo message\n", prog);
    fprintf(stderr, "  %s all message\n", prog);
}

static int join_args(char *dest, size_t size, char *argv[], int start, int argc)
{
    int i;
    size_t used = 0;

    if (start >= argc) {
        return -1;
    }

    dest[0] = '\0';
    for (i = start; i < argc; i++) {
        size_t len = strlen(argv[i]);

        if (used + len + (used ? 1u : 0u) + 1u > size) {
            return -1;
        }
        if (used != 0) {
            dest[used++] = ' ';
        }
        memcpy(dest + used, argv[i], len);
        used += len;
        dest[used] = '\0';
    }

    return 0;
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

static void init_loopback(struct sockaddr_in *sock)
{
    bzero(sock, sizeof(*sock));
    sock->sin_family = AF_INET;
    sock->sin_port = htons(PORT);
    sock->sin_addr.s_addr = inet_addr("127.0.0.1");
}

static int build_list_cmd(char *buf)
{
    buf[0] = '3';
    memcpy(buf + 1, MAGIC, MAGIC_LEN);
    return 6;
}

static int build_send_cmd(char *buf, size_t size, char *argv[], int argc)
{
    char message[LBUF];
    size_t pseudo_len;
    size_t msg_len;

    if (argc < 4) {
        return -1;
    }
    if (join_args(message, sizeof(message), argv, 3, argc) != 0) {
        return -1;
    }

    pseudo_len = strlen(argv[2]);
    msg_len = strlen(message);
    if (6 + pseudo_len + 1 + msg_len >= size) {
        return -1;
    }

    buf[0] = '4';
    memcpy(buf + 1, MAGIC, MAGIC_LEN);
    memcpy(buf + 6, argv[2], pseudo_len + 1);
    memcpy(buf + 6 + pseudo_len + 1, message, msg_len);
    return (int)(6 + pseudo_len + 1 + msg_len);
}

static int build_all_cmd(char *buf, size_t size, char *argv[], int argc)
{
    char message[LBUF];
    size_t msg_len;

    if (argc < 3) {
        return -1;
    }
    if (join_args(message, sizeof(message), argv, 2, argc) != 0) {
        return -1;
    }

    msg_len = strlen(message);
    if (6 + msg_len >= size) {
        return -1;
    }

    buf[0] = '5';
    memcpy(buf + 1, MAGIC, MAGIC_LEN);
    memcpy(buf + 6, message, msg_len);
    return (int)(6 + msg_len);
}

int main(int argc, char *argv[])
{
    int sid;
    int len;
    char buf[LBUF];
    struct sockaddr_in sock;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    sid = create_socket();
    if (sid < 0) {
        return 2;
    }

    init_loopback(&sock);

    if (strcmp(argv[1], "liste") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            close(sid);
            return 1;
        }
        len = build_list_cmd(buf);
    } else if (strcmp(argv[1], "send") == 0) {
        len = build_send_cmd(buf, sizeof(buf), argv, argc);
    } else if (strcmp(argv[1], "all") == 0) {
        len = build_all_cmd(buf, sizeof(buf), argv, argc);
    } else {
        print_usage(argv[0]);
        close(sid);
        return 1;
    }

    if (len < 0) {
        fprintf(stderr, "Commande invalide ou message trop long\n");
        close(sid);
        return 1;
    }

    if (sendto(sid, buf, (size_t)len, 0,
               (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        close(sid);
        return 3;
    }

    close(sid);
    printf("Commande envoyee a servbeuip\n");
    return 0;
}


