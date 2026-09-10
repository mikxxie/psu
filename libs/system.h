#ifndef SYSTEM_H
#define SYSTEM_H

int resolve_system_call(const char *token, char *output_buffer);
void get_system_time(char *buffer);
void get_system_date(char *buffer);
void get_system_os(char *buffer);
void get_system_hostname(char *buffer);
void get_system_username(char *buffer);
void get_system_ip4(char *buffer);
void get_system_ip6(char *buffer);

#endif
