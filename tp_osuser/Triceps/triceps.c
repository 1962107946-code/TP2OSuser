#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "creme.h"

#define MAXPAR 32

static int run = 1;
static int debug = 0;
static char *Mots[MAXPAR];
static int NMots = 0;

static char *join_words(int start)
{
    int i;
    size_t len = 0;
    char *msg;
    char *p;

    if (start >= NMots) {
        return NULL;
    }

    for (i = start; i < NMots; i++) {
        len += strlen(Mots[i]);
        if (i + 1 < NMots) {
            len++;
        }
    }

    msg = malloc(len + 1);
    if (msg == NULL) {
        return NULL;
    }

    p = msg;
    for (i = start; i < NMots; i++) {
        size_t l = strlen(Mots[i]);

        memcpy(p, Mots[i], l);
        p += l;
        if (i + 1 < NMots) {
            *p++ = ' ';
        }
    }
    *p = '\0';
    return msg;
}

static void Sortie(void)
{
    beuip_stop_server();
    run = 0;
}

static void handle_beuip(void)
{
    if (NMots == 3 && strcmp(Mots[1], "start") == 0) {
        beuip_start_server(Mots[2]);
        return;
    }
    if (NMots == 2 && strcmp(Mots[1], "stop") == 0) {
        beuip_stop_server();
        return;
    }
    fprintf(stderr, "Usage : beuip start pseudo | beuip stop\n");
}

static void handle_mess_to(void)
{
    char *msg;

    msg = join_words(3);
    if (msg == NULL) {
        fprintf(stderr, "Message invalide\n");
        return;
    }

    beuip_send_to(Mots[2], msg);
    free(msg);
}

static void handle_mess_all(void)
{
    char *msg;

    msg = join_words(2);
    if (msg == NULL) {
        fprintf(stderr, "Message invalide\n");
        return;
    }

    beuip_send_all(msg);
    free(msg);
}

static void handle_mess(void)
{
    if (NMots == 2 && strcmp(Mots[1], "list") == 0) {
        beuip_list();
        return;
    }
    if (NMots >= 4 && strcmp(Mots[1], "to") == 0) {
        handle_mess_to();
        return;
    }
    if (NMots >= 3 && strcmp(Mots[1], "all") == 0) {
        handle_mess_all();
        return;
    }
    fprintf(stderr, "Usage : mess list | mess to pseudo message | mess all message\n");
}

static void executeCommande(void)
{
    if (strcmp(Mots[0], "exit") == 0) {
        Sortie();
    } else if (strcmp(Mots[0], "beuip") == 0) {
        handle_beuip();
    } else if (strcmp(Mots[0], "mess") == 0) {
        handle_mess();
    } else {
        fprintf(stderr, "%s : commande inexistante !\n", Mots[0]);
    }
}

static int traiteCommande(char *buf)
{
    char *d;
    char *f;
    int mode = 1;

    d = buf;
    f = buf + strlen(buf);
    NMots = 0;

    while (d < f) {
        if (mode) {
            if (*d != ' ' && *d != '\t') {
                if (NMots == MAXPAR) {
                    break;
                }
                Mots[NMots++] = d;
                mode = 0;
            }
        } else if (*d == ' ' || *d == '\t') {
            *d = '\0';
            mode = 1;
        }
        d++;
    }

    if (debug) {
        printf("Il y a %d mots\n", NMots);
    }
    return NMots;
}

static int read_command(char **buf, size_t *lb)
{
    int n;

    printf("triceps> ");
    n = (int)getline(buf, lb, stdin);
    if (n == -1) {
        return -1;
    }
    if (n > 0 && (*buf)[n - 1] == '\n') {
        (*buf)[n - 1] = '\0';
        n--;
    }
    if (debug) {
        printf("Lu %d car.: %s\n", n, *buf);
    }
    return n;
}

static void print_debug_tokens(int n)
{
    int i;

    if (!debug || n <= 0) {
        return;
    }

    printf("La commande est %s !\n", Mots[0]);
    if (n > 1) {
        printf("Les parametres sont :\n");
        for (i = 1; i < n; i++) {
            printf("\t%s\n", Mots[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    char *buf = NULL;
    size_t lb = 0;
    int n;

    if (argc > 2) {
        fprintf(stderr, "Utilisation : %s [-d]\n", argv[0]);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        debug = 1;
    } else if (argc == 2) {
        fprintf(stderr, "parametre %s invalide !\n", argv[1]);
    }

    while (run) {
        n = read_command(&buf, &lb);
        if (n == -1) {
            break;
        }

        n = traiteCommande(buf);
        print_debug_tokens(n);
        if (n > 0) {
            executeCommande();
        }

        free(buf);
        buf = NULL;
        lb = 0;
    }

    free(buf);
    printf("Bye !\n");
    return 0;
}


