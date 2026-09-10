#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "filesys.h"

#define MAX_VALUE_LEN 4096

extern int is_filesys_loaded;
extern char* trim(char *str);
extern const char* get_variable(const char *name);
extern void evaluate_value(const char *expr, char *result);

static void extract_func_name(const char *token, char *out, size_t out_size) {
    const char *paren = strchr(token, '(');
    size_t len = paren ? (size_t)(paren - token) : strlen(token);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, token, len);
    out[len] = '\0';
    while (len > 0 && isspace((unsigned char)out[len - 1])) {
        out[--len] = '\0';
    }
}

static void resolve_name_arg(const char *arg, char *out, size_t out_size) {
    char tmp[MAX_VALUE_LEN];
    strncpy(tmp, arg ? arg : "", sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *s = trim(tmp);
    int len = (int)strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        s[len - 1] = '\0';
        strncpy(out, s + 1, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    const char *val = get_variable(s);
    if (val) {
        strncpy(out, val, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    strncpy(out, s, out_size - 1);
    out[out_size - 1] = '\0';
}

void filesys_write(const char *filename, const char *content) {
    FILE *file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", content);
        fclose(file);
    }
}

void filesys_touch(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file) fclose(file);
}

void filesys_read(const char *filename, char *buffer) {
    FILE *file = fopen(filename, "r");
    if (file) {
        size_t used = 0;
        buffer[0] = '\0';
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            size_t n = strlen(line);
            if (used + n >= MAX_VALUE_LEN) {
                n = MAX_VALUE_LEN - 1 - used;
                memcpy(buffer + used, line, n);
                used += n;
                buffer[used] = '\0';
                break;
            }
            memcpy(buffer + used, line, n + 1);
            used += n;
        }
        fclose(file);
    } else {
        strcpy(buffer, "File not found");
    }
}

void filesys_dir(char *buffer) {
    DIR *dir;
    struct dirent *entry;
    char result[MAX_VALUE_LEN];
    result[0] = '\0';
    size_t used = 0;
    
    dir = opendir(".");
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                size_t namelen = strlen(entry->d_name);
                if (used + namelen + 3 >= MAX_VALUE_LEN) break;
                memcpy(result + used, entry->d_name, namelen);
                memcpy(result + used + namelen, "  ", 2);
                used += namelen + 2;
                result[used] = '\0';
            }
        }
        closedir(dir);
        while (used > 0 && isspace((unsigned char)result[used - 1])) {
            result[--used] = '\0';
        }
        strcpy(buffer, result);
    } else {
        strcpy(buffer, "Cannot read directory");
    }
}

void filesys_pwd(char *buffer) {
    if (getcwd(buffer, MAX_VALUE_LEN) == NULL) {
        strcpy(buffer, "Unknown");
    }
}

void filesys_perm(const char *filename, char *buffer) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        char perms[12] = "";
        strcat(perms, (S_ISDIR(st.st_mode)) ? "d" : "-");
        strcat(perms, (st.st_mode & S_IRUSR) ? "r" : "-");
        strcat(perms, (st.st_mode & S_IWUSR) ? "w" : "-");
        strcat(perms, (st.st_mode & S_IXUSR) ? "x" : "-");
        strcat(perms, (st.st_mode & S_IRGRP) ? "r" : "-");
        strcat(perms, (st.st_mode & S_IWGRP) ? "w" : "-");
        strcat(perms, (st.st_mode & S_IXGRP) ? "x" : "-");
        strcat(perms, (st.st_mode & S_IROTH) ? "r" : "-");
        strcat(perms, (st.st_mode & S_IWOTH) ? "w" : "-");
        strcat(perms, (st.st_mode & S_IXOTH) ? "x" : "-");
        snprintf(buffer, MAX_VALUE_LEN, "%s %s", perms, filename);
    } else {
        strcpy(buffer, "File not found");
    }
}

int validate_filesys_command(const char *line) {
    char temp[MAX_VALUE_LEN];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *func_start = strstr(temp, "filesys.");
    if (!func_start) return 0;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 8;
        if (strcmp(func_name, "pwd") == 0 || strcmp(func_name, "dir") == 0) {
            return 1;
        }
        return 0;
    }
    
    *paren = '\0';
    char *func_name = func_start + 8;
    char *args = paren + 1;
    
    char *close_paren = strchr(args, ')');
    if (!close_paren) return 0;
    *close_paren = '\0';
    
    if (strcmp(func_name, "touch") == 0 || strcmp(func_name, "read") == 0 || 
        strcmp(func_name, "perm") == 0) {
        if (strlen(args) > 0) return 1;
    }
    else if (strcmp(func_name, "write") == 0) {
        char *remaining = close_paren + 1;
        while (isspace((unsigned char)*remaining)) remaining++;
        if (*remaining == '"' || *remaining == '\'') {
            return 1;
        }
        if (strlen(remaining) > 0) return 1;
    }
    
    return 0;
}

void execute_filesys_command(char *line) {
    char temp[MAX_VALUE_LEN];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *func_start = strstr(temp, "filesys.");
    if (!func_start) return;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 8;
        char result[MAX_VALUE_LEN];
        if (strcmp(func_name, "pwd") == 0) {
            filesys_pwd(result);
            printf("%s\n", result);
        } else if (strcmp(func_name, "dir") == 0) {
            filesys_dir(result);
            printf("%s\n", result);
        }
        return;
    }
    
    *paren = '\0';
    char *func_name = func_start + 8;
    char *args = paren + 1;
    char *close_paren = strchr(args, ')');
    if (!close_paren) return;
    *close_paren = '\0';
    
    args = trim(args);
    char filename[MAX_VALUE_LEN];
    resolve_name_arg(args, filename, sizeof(filename));
    char result[MAX_VALUE_LEN];
    
    if (strcmp(func_name, "touch") == 0) {
        filesys_touch(filename);
        printf("File created: %s\n", filename);
    }
    else if (strcmp(func_name, "read") == 0) {
        filesys_read(filename, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "perm") == 0) {
        filesys_perm(filename, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "write") == 0) {
        char *remaining = trim(close_paren + 1);
        char content[MAX_VALUE_LEN];
        evaluate_value(remaining, content);
        filesys_write(filename, content);
        printf("File written: %s\n", filename);
    }
}

int resolve_filesys_call(const char *token, char *output_buffer, char *arg) {
    char func[64];
    extract_func_name(token, func, sizeof(func));

    char filename[MAX_VALUE_LEN];
    if (arg) {
        resolve_name_arg(arg, filename, sizeof(filename));
    } else {
        filename[0] = '\0';
    }

    if (strcmp(func, "filesys.pwd") == 0) {
        filesys_pwd(output_buffer);
        return 1;
    }
    else if (strcmp(func, "filesys.dir") == 0) {
        filesys_dir(output_buffer);
        return 1;
    }
    else if (strcmp(func, "filesys.touch") == 0 && arg) {
        filesys_touch(filename);
        strcpy(output_buffer, "File created");
        return 1;
    }
    else if (strcmp(func, "filesys.write") == 0 && arg) {
        filesys_write(filename, "");
        strcpy(output_buffer, "File written");
        return 1;
    }
    else if (strcmp(func, "filesys.read") == 0 && arg) {
        filesys_read(filename, output_buffer);
        return 1;
    }
    else if (strcmp(func, "filesys.perm") == 0 && arg) {
        filesys_perm(filename, output_buffer);
        return 1;
    }
    return 0;
}

void load_filesys_library() {
    if (!is_filesys_loaded) {
        is_filesys_loaded = 1;
    }
}
