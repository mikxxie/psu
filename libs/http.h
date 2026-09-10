#ifndef HTTP_H
#define HTTP_H

void load_http_library();
int validate_http_command(const char *line);
void execute_http_command(char *line);
int resolve_http_call(const char *token, char *output_buffer, char *arg);
void http_get(const char *url, char *buffer);
void http_head(const char *url, char *buffer);

void http_server_start(int port);

#endif