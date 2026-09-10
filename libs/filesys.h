#ifndef FILESYS_H
#define FILESYS_H

void load_filesys_library();
int validate_filesys_command(const char *line);
void execute_filesys_command(char *line);
int resolve_filesys_call(const char *token, char *output_buffer, char *arg);
void filesys_pwd(char *buffer);
void filesys_dir(char *buffer);

#endif
