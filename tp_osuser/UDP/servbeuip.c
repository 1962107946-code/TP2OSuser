#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9998
#define LBUF 512
#define MAX_USERS 255
#define PSEUDO_LEN 64
#define MAGIC "BEUIP"
#define MAGIC_LEN 5
#define BROADCAST_IP "192.168.88.255"

#ifdef TRACE
#define TRACE_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACE_PRINT(...)
#endif

typedef struct {
    unsigned long ip;
    char pseudo[PSEUDO_LEN];
} User;

static User users[MAX_USERS];
static int user_count = 0;
static int sid = -1;
static char my_pseudo[PSEUDO_LEN];
static volatile sig_atomic_t stop_flag = 0;

static char *addrip(unsigned long addr)
{
    static char buf[16];

    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (unsigned int)((addr >> 24) & 0xFF),
             (unsigned int)((addr >> 16) & 0xFF),
             (unsigned int)((addr >> 8) & 0xFF),
             (unsigned int)(addr & 0xFF));
    return buf;
}

static int is_loopback_ip(unsigned long ip)
{
    return ip == 0x7F000001UL;
}

static int is_valid_msg(const char *buf, int len)
{
    if (len < 6) {
        return 0;
    }
    return memcmp(buf + 1, MAGIC, MAGIC_LEN) == 0;
}

static int find_user_by_ip(unsigned long ip)
{
    int i;

    for (i = 0; i < user_count; i++) {
        if (users[i].ip == ip) {
            return i;
        }
    }
    return -1;
}

static int find_user_by_pseudo(const char *pseudo)
{
    int i;

    for (i = 0; i < user_count; i++) {
        if (strcmp(users[i].pseudo, pseudo) == 0) {
            return i;
        }
    }
    return -1;
}

static int is_self_user(unsigned long ip, const char *pseudo)
{
    if (pseudo == NULL) {
        return 0;
    }
    if (strcmp(pseudo, my_pseudo) != 0) {
        return 0;
    }
    return !is_loopback_ip(ip);
}

static void store_user(unsigned long ip, const char *pseudo)
{
    int idx;

    if (pseudo == NULL || *pseudo == '\0') {
        return;
    }
    if (is_self_user(ip, pseudo)) {
        return;
    }

    idx = find_user_by_ip(ip);
    if (idx >= 0) {
        strncpy(users[idx].pseudo, pseudo, PSEUDO_LEN - 1);
        users[idx].pseudo[PSEUDO_LEN - 1] = '\0';
        return;
    }

    idx = find_user_by_pseudo(pseudo);
    if (idx >= 0) {
        users[idx].ip = ip;
        return;
    }

    if (user_count >= MAX_USERS) {
        fprintf(stderr, "Table pleine\n");
        return;
    }

    users[user_count].ip = ip;
    strncpy(users[user_count].pseudo, pseudo, PSEUDO_LEN - 1);
    users[user_count].pseudo[PSEUDO_LEN - 1] = '\0';
    TRACE_PRINT("[TRACE] ajout %s (%s)\n", users[user_count].pseudo, addrip(ip));
    user_count++;
}

static void remove_user(unsigned long ip)
{
    int idx;
    int i;

    idx = find_user_by_ip(ip);
    if (idx < 0) {
        return;
    }

    TRACE_PRINT("[TRACE] suppression %s (%s)\n",
                users[idx].pseudo, addrip(users[idx].ip));

    for (i = idx; i < user_count - 1; i++) {
        users[i] = users[i + 1];
    }
    user_count--;
}

static int build_simple_msg(char *buf, size_t size, char code, const char *payload)
{
    size_t payload_len = 0;

    if (size < 6) {
        return -1;
    }
    if (payload != NULL) {
        payload_len = strlen(payload);
    }
    if (6 + payload_len >= size) {
        return -1;
    }

    buf[0] = code;
    memcpy(buf + 1, MAGIC, MAGIC_LEN);
    if (payload_len > 0) {
        memcpy(buf + 6, payload, payload_len);
    }
    return (int)(6 + payload_len);
}

static int send_to_ip(unsigned long ip, char code, const char *payload)
{
    int len;
    char msg[LBUF];
    struct sockaddr_in dest;

    len = build_simple_msg(msg, sizeof(msg), code, payload);
    if (len < 0) {
        return -1;
    }

    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr.s_addr = htonl(ip);

    if (sendto(sid, msg, (size_t)len, 0,
               (struct sockaddr *)&dest, sizeof(dest)) == -1) {
        perror("sendto");
        return -1;
    }
    return 0;
}

static int send_broadcast(char code, const char *payload)
{
    int len;
    char msg[LBUF];
    struct sockaddr_in dest;

    len = build_simple_msg(msg, sizeof(msg), code, payload);
    if (len < 0) {
        return -1;
    }

    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    if (sendto(sid, msg, (size_t)len, 0,
               (struct sockaddr *)&dest, sizeof(dest)) == -1) {
        perror("sendto broadcast");
        return -1;
    }
    return 0;
}

static void print_users(void)
{
    int i;

    printf("Liste des utilisateurs (%d) :\n", user_count);
    for (i = 0; i < user_count; i++) {
        printf("- %s (%s)\n", users[i].pseudo, addrip(users[i].ip));
    }
}

static int parse_code4(char *payload, int payload_len,
                       char **dest_pseudo, char **message)
{
    char *sep;

    if (payload_len <= 1) {
        return -1;
    }

    sep = memchr(payload, '\0', (size_t)payload_len);
    if (sep == NULL || sep == payload) {
        return -1;
    }
    if (sep + 1 >= payload + payload_len) {
        return -1;
    }

    *dest_pseudo = payload;
    *message = sep + 1;
    return 0;
}

static void handle_code_1_or_2(char code, unsigned long ip, char *payload)
{
    if (*payload != '\0') {
        store_user(ip, payload);
    }
    if (code == '1') {
        send_to_ip(ip, '2', my_pseudo);
    }
}

static void handle_code_3(unsigned long ip)
{
    if (!is_loopback_ip(ip)) {
        fprintf(stderr, "Refus code 3 depuis %s\n", addrip(ip));
        return;
    }
    print_users();
}

static void handle_code_4(unsigned long ip, char *payload, int payload_len)
{
    int idx;
    char *dest_pseudo;
    char *message;

    if (!is_loopback_ip(ip)) {
        fprintf(stderr, "Refus code 4 depuis %s\n", addrip(ip));
        return;
    }
    if (parse_code4(payload, payload_len, &dest_pseudo, &message) != 0) {
        fprintf(stderr, "Message code 4 invalide\n");
        return;
    }

    idx = find_user_by_pseudo(dest_pseudo);
    if (idx < 0) {
        fprintf(stderr, "Pseudo inconnu : %s\n", dest_pseudo);
        return;
    }
    send_to_ip(users[idx].ip, '9', message);
}

static void handle_code_5(unsigned long ip, char *payload)
{
    int i;

    if (!is_loopback_ip(ip)) {
        fprintf(stderr, "Refus code 5 depuis %s\n", addrip(ip));
        return;
    }

    for (i = 0; i < user_count; i++) {
        if (strcmp(users[i].pseudo, my_pseudo) != 0) {
            send_to_ip(users[i].ip, '9', payload);
        }
    }
}

static void handle_code_9(unsigned long ip, char *payload)
{
    int idx;

    idx = find_user_by_ip(ip);
    if (idx >= 0) {
        printf("Message de %s : %s\n", users[idx].pseudo, payload);
    } else {
        printf("Message de %s : %s\n", addrip(ip), payload);
    }
}

static void sig_handler(int sig)
{
    (void)sig;
    stop_flag = 1;
}

static int prepare_socket(void)
{
    int yes = 1;
    struct sockaddr_in sockconf;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sid < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(sid, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        return -1;
    }

    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_BROADCAST");
        return -1;
    }

    bzero(&sockconf, sizeof(sockconf));
    sockconf.sin_family = AF_INET;
    sockconf.sin_port = htons(PORT);
    sockconf.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sid, (struct sockaddr *)&sockconf, sizeof(sockconf)) == -1) {
        perror("bind");
        return -1;
    }

    return 0;
}

static void handle_packet(char *buf, int len, struct sockaddr_in *sock)
{
    char code;
    unsigned long ip;

    if (!is_valid_msg(buf, len)) {
        return;
    }

    code = buf[0];
    ip = ntohl(sock->sin_addr.s_addr);
    buf[len] = '\0';

    TRACE_PRINT("[TRACE] recu code=%c de %s\n", code, addrip(ip));

    if (code == '0') {
        remove_user(ip);
    } else if (code == '1' || code == '2') {
        handle_code_1_or_2(code, ip, buf + 6);
    } else if (code == '3') {
        handle_code_3(ip);
    } else if (code == '4') {
        handle_code_4(ip, buf + 6, len - 6);
    } else if (code == '5') {
        handle_code_5(ip, buf + 6);
    } else if (code == '9') {
        handle_code_9(ip, buf + 6);
    }
}

int main(int argc, char *argv[])
{
    int n;
    char buf[LBUF + 1];
    socklen_t ls;
    struct sockaddr_in sock;

    if (argc != 2) {
        fprintf(stderr, "Utilisation : %s pseudo\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) >= sizeof(my_pseudo)) {
        fprintf(stderr, "Pseudo trop long\n");
        return 1;
    }

    strncpy(my_pseudo, argv[1], sizeof(my_pseudo) - 1);
    my_pseudo[sizeof(my_pseudo) - 1] = '\0';

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    if (prepare_socket() != 0) {
        return 2;
    }

    printf("servbeuip lance sur le port %d avec pseudo %s\n", PORT, my_pseudo);
    send_broadcast('1', my_pseudo);

    while (!stop_flag) {
        ls = sizeof(sock);
        n = recvfrom(sid, buf, LBUF, 0, (struct sockaddr *)&sock, &ls);
        if (n == -1) {
            if (errno == EINTR && stop_flag) {
                break;
            }
            perror("recvfrom");
            continue;
        }
        handle_packet(buf, n, &sock);
    }

    send_broadcast('0', my_pseudo);
    close(sid);
    printf("servbeuip arrete\n");
    return 0;
}

