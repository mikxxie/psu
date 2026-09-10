#ifndef NET_H
#define NET_H

void load_net_library();
int validate_net_command(const char *line);
void execute_net_command(char *line);
int resolve_net_call(const char *token, char *output_buffer, char *arg);
void net_iface_list(char *buffer);
void net_http_get(const char *url, char *buffer);
void net_http_head(const char *url, char *buffer);

#endif
