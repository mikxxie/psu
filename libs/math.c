#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "math.h"

#define MAX_VALUE_LEN 4096
#define MAX_NAME_LEN 32

extern void set_variable(const char *name, const char *value);
extern int is_math_loaded;

void remove_spaces(const char *input, char *output) {
    int j = 0;
    for (int i = 0; input[i]; i++) {
        if (!isspace((unsigned char)input[i])) {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

double parse_expression(const char **ptr, int *error);
double parse_term(const char **ptr, int *error);
double parse_factor(const char **ptr, int *error);

extern const char* get_variable(const char *name);

double parse_expression(const char **ptr, int *error) {
    double result = parse_term(ptr, error);
    if (*error) return 0;
    
    while (**ptr == '+' || **ptr == '-') {
        char op = **ptr;
        (*ptr)++;
        double next = parse_term(ptr, error);
        if (*error) return 0;
        if (op == '+') result += next;
        else result -= next;
    }
    return result;
}

double parse_term(const char **ptr, int *error) {
    double result = parse_factor(ptr, error);
    if (*error) return 0;
    
    while (**ptr == '*' || **ptr == '/') {
        char op = **ptr;
        (*ptr)++;
        double next = parse_factor(ptr, error);
        if (*error) return 0;
        if (op == '*') result *= next;
        else {
            if (next == 0) {
                *error = 1;
                return 0;
            }
            result /= next;
        }
    }
    return result;
}

double parse_factor(const char **ptr, int *error) {
    if (**ptr == '(') {
        (*ptr)++;
        double result = parse_expression(ptr, error);
        if (*error) return 0;
        if (**ptr == ')') {
            (*ptr)++;
            return result;
        } else {
            *error = 1;
            return 0;
        }
    }
    
    char *endptr;
    double result = strtod(*ptr, &endptr);
    if (*ptr == endptr) {
        const char *start = *ptr;
        while (isalnum((unsigned char)**ptr) || **ptr == '_' || **ptr == '.') (*ptr)++;
        if (*ptr > start) {
            char var_name[MAX_NAME_LEN];
            int len = *ptr - start;
            if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
            strncpy(var_name, start, len);
            var_name[len] = '\0';
            
            const char *var_val = get_variable(var_name);
            if (var_val) {
                return strtod(var_val, &endptr);
            } else {
                *error = 1;
                return 0;
            }
        }
        *error = 1;
        return 0;
    }
    *ptr = endptr;
    return result;
}

double evaluate_expression(const char *expr, int *error) {
    *error = 0;
    char clean[MAX_VALUE_LEN];
    remove_spaces(expr, clean);
    const char *ptr = clean;
    double result = parse_expression(&ptr, error);
    if (!*error && *ptr != '\0') *error = 1;
    return result;
}

int contains_math_expression(const char *str) {
    return strchr(str, '+') != NULL || strchr(str, '-') != NULL || 
           strchr(str, '*') != NULL || strchr(str, '/') != NULL;
}

void load_math_library() {
    if (!is_math_loaded) {
        is_math_loaded = 1;
        set_variable("math.pi", "3.141592653589793");
        set_variable("math.e", "2.718281828459045");
        set_variable("math.sqrt2", "1.414213562373095");
        set_variable("math.ln2", "0.693147180559945");
    }
}
