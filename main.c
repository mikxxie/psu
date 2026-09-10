#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "libs/system.h"
#include "libs/math.h"
#include "libs/filesys.h"
#include "libs/net.h"
#include "libs/http.h"

#define MAX_VARIABLES 50
#define MAX_NAME_LEN 32
#define MAX_VALUE_LEN 4096
#define MAX_LINES 500
#define MAX_FUNCTIONS 50
#define MAX_FUNC_NAME 32
#define MAX_LINE_LEN 512

typedef struct {
    char name[MAX_NAME_LEN];
    char value[MAX_VALUE_LEN];
} Variable;

typedef struct {
    char name[MAX_NAME_LEN];
    int line_start;
    int line_end;
} Function;

Variable memory[MAX_VARIABLES];
Function functions[MAX_FUNCTIONS];
int var_count = 0;
int func_count = 0;
int is_system_loaded = 0;
int is_math_loaded = 0;
int is_filesys_loaded = 0;
int is_net_loaded = 0;
int is_http_loaded = 0;
int has_errors = 0;
char lines[MAX_LINES][MAX_LINE_LEN];
int line_count = 0;

const char *current_filename = "";
int current_line_num = 0;

void execute_line(char *line, int line_num);
void execute_function(Function *func);
void evaluate_token(char *token, char *result);
void evaluate_concat(const char *expr, char *result);
void evaluate_value(const char *expr, char *result);
void extract_call_name(const char *token, char *out, int out_size);

char* trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void strip_comments(char *line) {
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '\'' && (i == 0 || line[i-1] != '\\')) {
            in_single_quote = !in_single_quote;
        } else if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            in_double_quote = !in_double_quote;
        }
        else if (line[i] == '/' && line[i+1] == '/' && !in_single_quote && !in_double_quote) {
            line[i] = '\0';
            return;
        }
    }
}

void set_variable(const char *name, const char *value) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(memory[i].name, name) == 0) {
            strncpy(memory[i].value, value, MAX_VALUE_LEN - 1);
            memory[i].value[MAX_VALUE_LEN - 1] = '\0';
            return;
        }
    }
    if (var_count < MAX_VARIABLES) {
        strncpy(memory[var_count].name, name, MAX_NAME_LEN - 1);
        memory[var_count].name[MAX_NAME_LEN - 1] = '\0';
        strncpy(memory[var_count].value, value, MAX_VALUE_LEN - 1);
        memory[var_count].value[MAX_VALUE_LEN - 1] = '\0';
        var_count++;
    }
}

const char* get_variable(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(memory[i].name, name) == 0) {
            return memory[i].value;
        }
    }
    return NULL;
}

int is_numeric(const char *str) {
    int has_decimal = 0;
    if (*str == '-') str++;
    if (*str == '\0') return 0;
    while (*str) {
        if (*str == '.') {
            if (has_decimal) return 0;
            has_decimal = 1;
        } else if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int is_url(const char *str) {
    return (strncmp(str, "http://", 7) == 0 || strncmp(str, "https://", 8) == 0);
}

int is_quoted(const char *str) {
    int len = strlen(str);
    return (len >= 2 && ((str[0] == '"' && str[len-1] == '"') || 
            (str[0] == '\'' && str[len-1] == '\'')));
}

int is_single_quoted_string(const char *str) {
    int len = strlen(str);
    if (len < 2) return 0;
    char q = str[0];
    if (q != '\'' && q != '"') return 0;
    if (str[len - 1] != q) return 0;
    for (int i = 1; i < len - 1; i++) {
        if (str[i] == q) return 0;
    }
    return 1;
}

void strip_quotes(char *str) {
    int len = strlen(str);
    if (len >= 2) {
        if ((str[0] == '"' && str[len-1] == '"') || 
            (str[0] == '\'' && str[len-1] == '\'')) {
            memmove(str, str + 1, len - 1);
            str[len - 2] = '\0';
        }
    }
}

void extract_call_name(const char *token, char *out, int out_size) {
    const char *paren = strchr(token, '(');
    int len = paren ? (int)(paren - token) : (int)strlen(token);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, token, len);
    out[len] = '\0';
    while (len > 0 && isspace((unsigned char)out[len - 1])) {
        out[--len] = '\0';
    }
}

Function* find_function(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return &functions[i];
        }
    }
    return NULL;
}

void execute_function(Function *func) {
    for (int i = func->line_start; i <= func->line_end; i++) {
        char line_copy[MAX_LINE_LEN];
        strcpy(line_copy, lines[i]);
        execute_line(line_copy, i + 1);
    }
}

int is_filesys_command(const char *line) {
    return strncmp(line, "filesys.", 8) == 0;
}

int is_net_command(const char *line) {
    return strncmp(line, "net.", 4) == 0;
}

int is_http_command(const char *line) {
    return strncmp(line, "http.", 5) == 0;
}

int is_system_command(const char *line) {
    return strncmp(line, "system.", 7) == 0;
}

char* extract_arg(const char *call) {
    static char arg[MAX_VALUE_LEN];
    char *paren = strchr(call, '(');
    if (!paren) {
        return NULL;
    }
    
    char *end_paren = strrchr(paren + 1, ')');
    if (!end_paren) {
        return NULL;
    }
    
    int len = end_paren - (paren + 1);
    if (len < 0 || len >= MAX_VALUE_LEN - 1) {
        return NULL;
    }
    
    strncpy(arg, paren + 1, len);
    arg[len] = '\0';
    
    char *trimmed = trim(arg);
    if (trimmed != arg) {
        memmove(arg, trimmed, strlen(trimmed) + 1);
    }
    
    return arg;
}

int is_command_without_args(const char *token) {
    return (is_system_command(token) && strchr(token, '(') == NULL) ||
           (is_net_command(token) && strchr(token, '(') == NULL) ||
           (is_filesys_command(token) && strchr(token, '(') == NULL) ||
           (is_http_command(token) && strchr(token, '(') == NULL);
}

void evaluate_command_without_args(const char *token, char *result) {
    result[0] = '\0';
    
    if (is_system_command(token)) {
        if (resolve_system_call(token, result)) {
            return;
        }
    }
    
    if (is_net_command(token)) {
        if (resolve_net_call(token, result, NULL)) {
            return;
        }
    }
    
    if (is_filesys_command(token)) {
        if (resolve_filesys_call(token, result, NULL)) {
            return;
        }
    }
    
    strcpy(result, "Unknown command");
}

static void append_capped(char *dst, size_t cap, const char *src) {
    size_t used = strlen(dst);
    if (used >= cap - 1) return;
    size_t add = strlen(src);
    if (used + add >= cap) add = cap - 1 - used;
    memcpy(dst + used, src, add);
    dst[used + add] = '\0';
}

void evaluate_token(char *token, char *result) {
    result[0] = '\0';
    
    char *trimmed_token = trim(token);
    
    if (is_single_quoted_string(trimmed_token)) {
        int len = strlen(trimmed_token);
        trimmed_token[len - 1] = '\0';
        strcpy(result, trimmed_token + 1);
        return;
    }
    
    const char *val = get_variable(trimmed_token);
    if (val) {
        strncpy(result, val, MAX_VALUE_LEN - 1);
        result[MAX_VALUE_LEN - 1] = '\0';
        return;
    }
    
    if (is_http_command(trimmed_token) && strchr(trimmed_token, '(') != NULL) {
        char cmd_result[MAX_VALUE_LEN];
        char func_name[64];
        extract_call_name(trimmed_token, func_name, sizeof(func_name));
        char *arg = extract_arg(trimmed_token);
        if (arg) {
            if (is_quoted(arg)) strip_quotes(arg);
            if (resolve_http_call(func_name, cmd_result, arg)) {
                strcpy(result, cmd_result);
                return;
            }
        }
    }
    
    if (is_net_command(trimmed_token) && strchr(trimmed_token, '(') != NULL) {
        char cmd_result[MAX_VALUE_LEN];
        char func_name[64];
        extract_call_name(trimmed_token, func_name, sizeof(func_name));
        char *arg = extract_arg(trimmed_token);
        if (arg) {
            if (is_quoted(arg)) strip_quotes(arg);
            if (resolve_net_call(func_name, cmd_result, arg)) {
                strcpy(result, cmd_result);
                return;
            }
        }
    }
    
    if (is_filesys_command(trimmed_token) && strchr(trimmed_token, '(') != NULL) {
        char cmd_result[MAX_VALUE_LEN];
        char func_name[64];
        extract_call_name(trimmed_token, func_name, sizeof(func_name));
        char *arg = extract_arg(trimmed_token);
        if (arg) {
            if (is_quoted(arg)) strip_quotes(arg);
            if (resolve_filesys_call(func_name, cmd_result, arg)) {
                strcpy(result, cmd_result);
                return;
            }
        }
    }
    
    if (is_system_command(trimmed_token)) {
        char cmd_result[MAX_VALUE_LEN];
        char func_name[64];
        extract_call_name(trimmed_token, func_name, sizeof(func_name));
        if (resolve_system_call(func_name, cmd_result)) {
            strcpy(result, cmd_result);
            return;
        }
    }
    
    if (is_command_without_args(trimmed_token)) {
        evaluate_command_without_args(trimmed_token, result);
        return;
    }
    
    if (is_math_loaded && contains_math_expression(trimmed_token)) {
        int error = 0;
        double math_val = evaluate_expression(trimmed_token, &error);
        if (!error) {
            snprintf(result, MAX_VALUE_LEN, "%g", math_val);
            return;
        }
    }
    
    if (is_numeric(trimmed_token)) {
        strcpy(result, trimmed_token);
        return;
    }
    
    strncpy(result, trimmed_token, MAX_VALUE_LEN - 1);
    result[MAX_VALUE_LEN - 1] = '\0';
}

void evaluate_concat(const char *expr, char *result) {
    char temp2[MAX_VALUE_LEN * 2];
    strncpy(temp2, expr, sizeof(temp2) - 1);
    temp2[sizeof(temp2) - 1] = '\0';
    result[0] = '\0';
    
    char *start = temp2;
    char *pos;
    while ((pos = strstr(start, "++")) != NULL) {
        *pos = '\0';
        char *trimmed_token = trim(start);
        if (strlen(trimmed_token) > 0) {
            char token_result[MAX_VALUE_LEN];
            evaluate_token(trimmed_token, token_result);
            append_capped(result, MAX_VALUE_LEN, token_result);
        }
        start = pos + 2;
    }
    char *trimmed_token = trim(start);
    if (strlen(trimmed_token) > 0) {
        char token_result[MAX_VALUE_LEN];
        evaluate_token(trimmed_token, token_result);
        append_capped(result, MAX_VALUE_LEN, token_result);
    }
}

void evaluate_value(const char *expr, char *result) {
    char temp[MAX_VALUE_LEN * 2];
    strncpy(temp, expr, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    char *trimmed = trim(temp);
    if (strstr(trimmed, "++")) {
        evaluate_concat(trimmed, result);
        return;
    }
    evaluate_token(trimmed, result);
}

void evaluate_echo_args(const char *arg, char *result) {
    result[0] = '\0';
    const char *p = arg;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '\'' || *p == '"') {
            char q = *p++;
            char tmp[MAX_VALUE_LEN];
            int i = 0;
            while (*p && *p != q && i < MAX_VALUE_LEN - 1) {
                tmp[i++] = *p++;
            }
            tmp[i] = '\0';
            if (*p == q) p++;
            append_capped(result, MAX_VALUE_LEN, tmp);
        } else {
            char tmp[MAX_VALUE_LEN];
            int i = 0;
            while (*p && !isspace((unsigned char)*p) && i < MAX_VALUE_LEN - 1) {
                tmp[i++] = *p++;
            }
            tmp[i] = '\0';
            char val[MAX_VALUE_LEN];
            evaluate_token(tmp, val);
            append_capped(result, MAX_VALUE_LEN, val);
        }
    }
}

void validate_line(char *line, int line_num) {
    current_line_num = line_num;
    char line_copy[MAX_LINE_LEN];
    strcpy(line_copy, line);
    strip_comments(line_copy);
    char *trimmed = trim(line_copy);
    
    if (strlen(trimmed) == 0 || strcmp(trimmed, "psstart") == 0) return;

    if (strncmp(trimmed, "psload ", 7) == 0) {
        char *lib_name = trim(trimmed + 7);
        if (strcmp(lib_name, "system") == 0) {
            is_system_loaded = 1;
            return;
        } else if (strcmp(lib_name, "math") == 0) {
            is_math_loaded = 1;
            load_math_library();
            return;
        } else if (strcmp(lib_name, "filesys") == 0) {
            is_filesys_loaded = 1;
            load_filesys_library();
            return;
        } else if (strcmp(lib_name, "net") == 0) {
            is_net_loaded = 1;
            load_net_library();
            return;
        } else if (strcmp(lib_name, "http") == 0) {
            is_http_loaded = 1;
            load_http_library();
            return;
        } else {
            printf("Compilation Error in %s on line %d: Unknown library '%s'\n", current_filename, current_line_num, lib_name);
            has_errors = 1;
        }
        return;
    }

    if (strncmp(trimmed, "def ", 4) == 0) {
        char *func_name = trim(trimmed + 4);
        
        if (strlen(func_name) == 0) {
            printf("Syntax Error in %s on line %d: Missing function name after 'def'.\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }
        
        if (func_count >= MAX_FUNCTIONS) {
            printf("Compilation Error in %s on line %d: Too many function definitions (max %d).\n", current_filename, current_line_num, MAX_FUNCTIONS);
            has_errors = 1;
            return;
        }
        
        if (find_function(func_name) != NULL) {
            printf("Compilation Error in %s on line %d: Function '%s' is already defined.\n", current_filename, current_line_num, func_name);
            has_errors = 1;
            return;
        }
        
        int end_line = -1;
        for (int i = line_num; i < line_count; i++) {
            char temp_line[MAX_LINE_LEN];
            strcpy(temp_line, lines[i]);
            strip_comments(temp_line);
            if (strcmp(trim(temp_line), "enddef") == 0) {
                end_line = i;
                break;
            }
        }
        
        if (end_line == -1) {
            printf("Syntax Error in %s on line %d: Function '%s' is missing 'enddef'.\n", current_filename, current_line_num, func_name);
            has_errors = 1;
            return;
        }
        
        strcpy(functions[func_count].name, func_name);
        functions[func_count].line_start = line_num;
        functions[func_count].line_end = end_line - 1;
        func_count++;
        
        return;
    }
    
    if (strcmp(trimmed, "enddef") == 0) {
        return;
    }

    for (int i = 0; i < func_count; i++) {
        if (strcmp(trimmed, functions[i].name) == 0) {
            return;
        }
    }

    if (is_filesys_command(trimmed)) {
        if (!is_filesys_loaded) {
            printf("Runtime Error in %s on line %d: 'filesys' is undefined. Did you forget 'psload filesys'?\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }
        if (!validate_filesys_command(trimmed)) {
            printf("Syntax Error in %s on line %d: Invalid filesystem command '%s'.\n", current_filename, current_line_num, trimmed);
            has_errors = 1;
        }
        return;
    }

    if (is_net_command(trimmed)) {
        if (!is_net_loaded) {
            printf("Runtime Error in %s on line %d: 'net' is undefined. Did you forget 'psload net'?\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }
        if (!validate_net_command(trimmed)) {
            printf("Syntax Error in %s on line %d: Invalid net command '%s'.\n", current_filename, current_line_num, trimmed);
            has_errors = 1;
        }
        return;
    }

    if (is_http_command(trimmed)) {
        if (!is_http_loaded) {
            printf("Runtime Error in %s on line %d: 'http' is undefined. Did you forget 'psload http'?\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }
        if (!validate_http_command(trimmed)) {
            printf("Syntax Error in %s on line %d: Invalid http command '%s'.\n", current_filename, current_line_num, trimmed);
            has_errors = 1;
        }
        return;
    }

    if (strncmp(trimmed, "var ", 4) == 0) {
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            printf("Parse Error in %s on line %d: Expected '=' after variable name.\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }
        *eq = '\0';
        char *var_name = trim(trimmed + 4);
        char *var_val = trim(eq + 1);

        if (strlen(var_name) == 0) {
            printf("Parse Error in %s on line %d: Missing variable name before '='.\n", current_filename, current_line_num);
            has_errors = 1;
            return;
        }

        if (strstr(var_val, "++")) {
            return;
        }
        else if (is_single_quoted_string(var_val)) {
            return;
        }
        else if (is_system_command(var_val)) {
            if (!is_system_loaded) {
                printf("Runtime Error in %s on line %d: 'system' is undefined. Did you forget 'psload system'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            char dummy[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(var_val, func_name, sizeof(func_name));
            if (!resolve_system_call(func_name, dummy)) {
                printf("Runtime Error in %s on line %d: '%s' is not a valid function.\n", current_filename, current_line_num, var_val);
                has_errors = 1;
            }
            return;
        }
        else if (is_filesys_command(var_val)) {
            if (!is_filesys_loaded) {
                printf("Runtime Error in %s on line %d: 'filesys' is undefined. Did you forget 'psload filesys'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            return;
        }
        else if (is_net_command(var_val)) {
            if (!is_net_loaded) {
                printf("Runtime Error in %s on line %d: 'net' is undefined. Did you forget 'psload net'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            return;
        }
        else if (is_http_command(var_val)) {
            if (!is_http_loaded) {
                printf("Runtime Error in %s on line %d: 'http' is undefined. Did you forget 'psload http'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            char *arg = extract_arg(var_val);
            if (arg && is_url(arg) && !is_quoted(arg)) {
                printf("Parse Error in %s on line %d: URLs must be quoted: '%s'. Use \"%s\"\n", current_filename, current_line_num, arg, arg);
                has_errors = 1;
                return;
            }
            return;
        }
        else if (contains_math_expression(var_val)) {
            if (!is_math_loaded) {
                printf("Runtime Error in %s on line %d: Math expression support is undefined. Did you forget 'psload math'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            return;
        }
        else {
            const char *existing = get_variable(var_val);
            if (!existing && !is_numeric(var_val)) {
                printf("Runtime Error in %s on line %d: Variable '%s' is undefined.\n", current_filename, current_line_num, var_val);
                has_errors = 1;
            }
            return;
        }
    }
    else if (strncmp(trimmed, "echo ", 5) == 0) {
        char *arg = trim(trimmed + 5);
        
        if (strstr(arg, "++")) {
            return;
        }
        
        if (is_single_quoted_string(arg)) {
            return;
        } 
        else if (is_system_command(arg)) {
            if (!is_system_loaded) {
                printf("Runtime Error in %s on line %d: 'system' is undefined. Did you forget 'psload system'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            char dummy[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(arg, func_name, sizeof(func_name));
            if (!resolve_system_call(func_name, dummy)) {
                printf("Runtime Error in %s on line %d: '%s' is not a valid function.\n", current_filename, current_line_num, arg);
                has_errors = 1;
            }
            return;
        }
        else if (is_filesys_command(arg)) {
            if (!is_filesys_loaded) {
                printf("Runtime Error in %s on line %d: 'filesys' is undefined. Did you forget 'psload filesys'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            if (!validate_filesys_command(arg)) {
                printf("Syntax Error in %s on line %d: Invalid filesystem command '%s'.\n", current_filename, current_line_num, arg);
                has_errors = 1;
            }
            return;
        }
        else if (is_net_command(arg)) {
            if (!is_net_loaded) {
                printf("Runtime Error in %s on line %d: 'net' is undefined. Did you forget 'psload net'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            if (!validate_net_command(arg)) {
                printf("Syntax Error in %s on line %d: Invalid net command '%s'.\n", current_filename, current_line_num, arg);
                has_errors = 1;
            }
            return;
        }
        else if (is_http_command(arg)) {
            if (!is_http_loaded) {
                printf("Runtime Error in %s on line %d: 'http' is undefined. Did you forget 'psload http'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            if (!validate_http_command(arg)) {
                printf("Syntax Error in %s on line %d: Invalid http command '%s'.\n", current_filename, current_line_num, arg);
                has_errors = 1;
            }
            return;
        }
        else if (contains_math_expression(arg)) {
            if (!is_math_loaded) {
                printf("Runtime Error in %s on line %d: Math expression support is undefined. Did you forget 'psload math'?\n", current_filename, current_line_num);
                has_errors = 1;
                return;
            }
            return;
        }
        else {
            return;
        }
    }
    else {
        int is_func = 0;
        for (int i = 0; i < func_count; i++) {
            if (strcmp(trimmed, functions[i].name) == 0) {
                is_func = 1;
                break;
            }
        }
        if (!is_func) {
            printf("Syntax Error in %s on line %d: Unknown command statement '%s'.\n", current_filename, current_line_num, trimmed);
            has_errors = 1;
        }
    }
}

void execute_line(char *line, int line_num) {
    current_line_num = line_num;
    char line_copy[MAX_LINE_LEN];
    strcpy(line_copy, line);
    strip_comments(line_copy);
    char *trimmed = trim(line_copy);
    
    if (strlen(trimmed) == 0 || strcmp(trimmed, "psstart") == 0 || 
        strcmp(trimmed, "enddef") == 0 || strncmp(trimmed, "def ", 4) == 0) {
        return;
    }

    if (strncmp(trimmed, "psload ", 7) == 0) {
        char *lib_name = trim(trimmed + 7);
        if (strcmp(lib_name, "system") == 0) {
            is_system_loaded = 1;
        } else if (strcmp(lib_name, "math") == 0) {
            load_math_library();
        } else if (strcmp(lib_name, "filesys") == 0) {
            load_filesys_library();
        } else if (strcmp(lib_name, "net") == 0) {
            load_net_library();
        } else if (strcmp(lib_name, "http") == 0) {
            load_http_library();
        }
        return;
    }

    for (int i = 0; i < func_count; i++) {
        if (strcmp(trimmed, functions[i].name) == 0) {
            execute_function(&functions[i]);
            return;
        }
    }

    if (is_filesys_command(trimmed)) {
        execute_filesys_command(trimmed);
        return;
    }

    if (is_net_command(trimmed)) {
        execute_net_command(trimmed);
        return;
    }

    if (is_http_command(trimmed)) {
        execute_http_command(trimmed);
        return;
    }

    if (strncmp(trimmed, "var ", 4) == 0) {
        char *eq = strchr(trimmed, '=');
        if (!eq) return;
        *eq = '\0';
        char *var_name = trim(trimmed + 4);
        char *var_val = trim(eq + 1);

        if (strstr(var_val, "++")) {
            char result[MAX_VALUE_LEN];
            evaluate_concat(var_val, result);
            set_variable(var_name, result);
        }
        else if (is_single_quoted_string(var_val)) {
            var_val[strlen(var_val) - 1] = '\0';
            set_variable(var_name, var_val + 1);
        }
        else if (is_system_command(var_val)) {
            char system_res[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(var_val, func_name, sizeof(func_name));
            resolve_system_call(func_name, system_res);
            set_variable(var_name, system_res);
        }
        else if (is_filesys_command(var_val)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(var_val, func_name, sizeof(func_name));
            char *arg = extract_arg(var_val);
            if (arg && is_quoted(arg)) strip_quotes(arg);
            if (resolve_filesys_call(func_name, result, arg)) {
                set_variable(var_name, result);
            }
        }
        else if (is_net_command(var_val)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(var_val, func_name, sizeof(func_name));
            char *arg = extract_arg(var_val);
            if (arg && is_quoted(arg)) strip_quotes(arg);
            if (resolve_net_call(func_name, result, arg)) {
                set_variable(var_name, result);
            }
        }
        else if (is_http_command(var_val)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(var_val, func_name, sizeof(func_name));
            char *arg = extract_arg(var_val);
            if (arg) {
                if (is_quoted(arg)) {
                    strip_quotes(arg);
                }
                if (resolve_http_call(func_name, result, arg)) {
                    set_variable(var_name, result);
                } else {
                    set_variable(var_name, "Error: HTTP request failed");
                }
            } else {
                set_variable(var_name, "Error: Invalid HTTP call syntax");
            }
        }
        else if (contains_math_expression(var_val)) {
            int error;
            double math_result = evaluate_expression(var_val, &error);
            char result_str[MAX_VALUE_LEN];
            snprintf(result_str, MAX_VALUE_LEN, "%g", math_result);
            set_variable(var_name, result_str);
        }
        else {
            const char *existing = get_variable(var_val);
            if (existing) {
                set_variable(var_name, existing);
            } else if (is_numeric(var_val)) {
                set_variable(var_name, var_val);
            }
        }
    }
    else if (strncmp(trimmed, "echo ", 5) == 0) {
        char *arg = trim(trimmed + 5);
        
        if (strstr(arg, "++")) {
            char result[MAX_VALUE_LEN];
            evaluate_concat(arg, result);
            printf("%s\n", result);
            return;
        }
        
        if (is_single_quoted_string(arg)) {
            arg[strlen(arg) - 1] = '\0';
            printf("%s\n", arg + 1);
        } 
        else if (is_system_command(arg)) {
            char system_res[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(arg, func_name, sizeof(func_name));
            resolve_system_call(func_name, system_res);
            printf("%s\n", system_res);
        }
        else if (is_filesys_command(arg)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(arg, func_name, sizeof(func_name));
            char *farg = extract_arg(arg);
            if (farg && is_quoted(farg)) strip_quotes(farg);
            if (strchr(arg, '(') == NULL || resolve_filesys_call(func_name, result, farg)) {
                if (strchr(arg, '(') == NULL) {
                    execute_filesys_command(arg);
                } else {
                    printf("%s\n", result);
                }
            } else {
                execute_filesys_command(arg);
            }
        }
        else if (is_net_command(arg)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(arg, func_name, sizeof(func_name));
            char *narg = extract_arg(arg);
            if (narg && is_quoted(narg)) strip_quotes(narg);
            if (resolve_net_call(func_name, result, narg)) {
                printf("%s\n", result);
            } else {
                execute_net_command(arg);
            }
        }
        else if (is_http_command(arg)) {
            char result[MAX_VALUE_LEN];
            char func_name[64];
            extract_call_name(arg, func_name, sizeof(func_name));
            char *harg = extract_arg(arg);
            if (harg && is_quoted(harg)) strip_quotes(harg);
            if (resolve_http_call(func_name, result, harg)) {
                printf("%s\n", result);
            } else {
                execute_http_command(arg);
            }
        }
        else if (contains_math_expression(arg)) {
            int error;
            double math_result = evaluate_expression(arg, &error);
            printf("%g\n", math_result);
        }
        else if (strchr(arg, '\'') || strchr(arg, '"')) {
            char result[MAX_VALUE_LEN];
            evaluate_echo_args(arg, result);
            printf("%s\n", result);
        }
        else {
            const char *val = get_variable(arg);
            if (val) {
                printf("%s\n", val);
            } else if (is_numeric(arg)) {
                printf("%s\n", arg);
            } else {
                printf("Runtime Error in %s on line %d: Undefined identifier '%s'.\n", current_filename, current_line_num, arg);
                has_errors = 1;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename.psu>\n", argv[0]);
        return 1;
    }

    current_filename = argv[1];
    FILE *file = fopen(current_filename, "r");
    if (!file) {
        printf("Fatal Error: Could not open source file %s\n", current_filename);
        return 1;
    }

    line_count = 0;
    int has_psstart = 0;

    while (fgets(lines[line_count], sizeof(lines[0]), file)) {
        lines[line_count][strcspn(lines[line_count], "\r\n")] = 0;
        
        char temp_line[MAX_LINE_LEN];
        strcpy(temp_line, lines[line_count]);
        strip_comments(temp_line);
        if (strcmp(trim(temp_line), "psstart") == 0) {
            has_psstart = 1;
        }
        line_count++;
        if (line_count >= MAX_LINES) break;
    }
    fclose(file);

    if (!has_psstart) {
        printf("Execution Error: Code compilation rejected. Missing 'psstart' initialization token at the end of %s.\n", current_filename);
        return 1;
    }

    is_system_loaded = 0;
    is_math_loaded = 0;
    is_filesys_loaded = 0;
    is_net_loaded = 0;
    is_http_loaded = 0;
    var_count = 0;
    func_count = 0;
    
    for (int i = 0; i < line_count; i++) {
        validate_line(lines[i], i + 1);
    }

    if (has_errors) {
        return 1;
    }

    is_system_loaded = 0;
    is_math_loaded = 0;
    is_filesys_loaded = 0;
    is_net_loaded = 0;
    is_http_loaded = 0;
    var_count = 0;
    func_count = 0;
    
    for (int i = 0; i < line_count; i++) {
        char line_copy[MAX_LINE_LEN];
        strcpy(line_copy, lines[i]);
        strip_comments(line_copy);
        char *trimmed = trim(line_copy);
        
        if (strncmp(trimmed, "def ", 4) == 0) {
            char *func_name = trim(trimmed + 4);
            if (strlen(func_name) > 0 && func_count < MAX_FUNCTIONS) {
                strcpy(functions[func_count].name, func_name);
                functions[func_count].line_start = i + 1;
                for (int j = i + 1; j < line_count; j++) {
                    char temp_line[MAX_LINE_LEN];
                    strcpy(temp_line, lines[j]);
                    strip_comments(temp_line);
                    if (strcmp(trim(temp_line), "enddef") == 0) {
                        functions[func_count].line_end = j - 1;
                        break;
                    }
                }
                func_count++;
            }
            while (i < line_count) {
                char temp_line[MAX_LINE_LEN];
                strcpy(temp_line, lines[i]);
                strip_comments(temp_line);
                if (strcmp(trim(temp_line), "enddef") == 0) {
                    break;
                }
                i++;
            }
            continue;
        }
        
        execute_line(lines[i], i + 1);
    }

    return 0;
}
