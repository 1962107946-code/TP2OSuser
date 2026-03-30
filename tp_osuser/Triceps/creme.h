#ifndef CREME_H
#define CREME_H

int beuip_start_server(const char *pseudo);
int beuip_stop_server(void);
int beuip_list(void);
int beuip_send_to(const char *pseudo, const char *msg);
int beuip_send_all(const char *msg);

#endif
