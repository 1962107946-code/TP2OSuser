#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <limits.h>
#include <libgen.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "creme.h"

#define PORT 9998
#define LBUF 512
#define MAGIC "BEUIP"
#define MAGIC_LEN 5

#ifdef TRACE
#define TRACE_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACE_PRINT(...)
#endif

static pid_t server_pid = -1;

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

static int build_simple_cmd(char *buf, size_t size, char code, const char *payload)
{
    size_t payload_len = 0;

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

static int build_send_cmd(char *buf, size_t size,
                          const char *pseudo, const char *msg)
{
    size_t pseudo_len;
    size_t msg_len;

    pseudo_len = strlen(pseudo);
    msg_len = strlen(msg);
    if (6 + pseudo_len + 1 + msg_len >= size) {
        return -1;
    }

    buf[0] = '4';
    memcpy(buf + 1, MAGIC, MAGIC_LEN);
    memcpy(buf + 6, pseudo, pseudo_len + 1);
    memcpy(buf + 6 + pseudo_len + 1, msg, msg_len);
    return (int)(6 + pseudo_len + 1 + msg_len);
}

static int send_cmd_buffer(const char *buf, int len)
{
    int sid;
    struct sockaddr_in sock;

    sid = create_socket();
    if (sid < 0) {
        return -1;
    }

    init_loopback(&sock);
    if (sendto(sid, buf, (size_t)len, 0,
               (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        close(sid);
        return -1;
    }

    close(sid);
    return 0;
}

static int send_local_command(char code, const char *a, const char *b)
{
    int len;
    char buf[LBUF];

    if (code == '4') {
        if (a == NULL || b == NULL) {
            return -1;
        }
        len = build_send_cmd(buf, sizeof(buf), a, b);
    } else {
        len = build_simple_cmd(buf, sizeof(buf), code, a);
    }

    if (len < 0) {
        fprintf(stderr, "Commande trop longue\n");
        return -1;
    }
    return send_cmd_buffer(buf, len);
}

static int exec_servbeuip(const char *pseudo)
{
    char exe_path[PATH_MAX];
    char dirbuf[PATH_MAX];
    char candidate[PATH_MAX];
    ssize_t n;

    n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n > 0) {
        exe_path[n] = '\0';
        strncpy(dirbuf, exe_path, sizeof(dirbuf) - 1);
        dirbuf[sizeof(dirbuf) - 1] = '\0';
        snprintf(candidate, sizeof(candidate), "%s/../UDP/servbeuip",
                 dirname(dirbuf));
        execl(candidate, "servbeuip", pseudo, (char *)NULL);
    }

    execl("../UDP/servbeuip", "servbeuip", pseudo, (char *)NULL);
    execl("../servbeuip", "servbeuip", pseudo, (char *)NULL);
    execl("./servbeuip", "servbeuip", pseudo, (char *)NULL);
    perror("execl servbeuip");
    return -1;
}

int beuip_start_server(const char *pseudo)
{
    if (pseudo == NULL || *pseudo == '\0') {
        fprintf(stderr, "Pseudo invalide\n");
        return -1;
    }
    if (server_pid > 0) {
        fprintf(stderr, "servbeuip deja lance\n");
        return -1;
    }

    server_pid = fork();
    if (server_pid < 0) {
        perror("fork");
        return -1;
    }

    if (server_pid == 0) {
        exec_servbeuip(pseudo);
        _exit(1);
    }

    TRACE_PRINT("[TRACE] servbeuip pid=%d\n", server_pid);
    return 0;
}

int beuip_stop_server(void)
{
    if (server_pid <= 0) {
        fprintf(stderr, "aucun servbeuip lance\n");
        return -1;
    }

    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, 0);
    server_pid = -1;
    return 0;
}

int beuip_list(void)
{
    return send_local_command('3', NULL, NULL);
}

int beuip_send_to(const char *pseudo, const char *msg)
{
    if (pseudo == NULL || msg == NULL) {
        return -1;
    }
    return send_local_command('4', pseudo, msg);
}

int beuip_send_all(const char *msg)
{
    if (msg == NULL) {
        return -1;
    }
    return send_local_command('5', msg, NULL);
}

