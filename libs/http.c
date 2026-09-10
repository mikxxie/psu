#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <strings.h>
#include "http.h"
#include "net.h"

#define MAX_VALUE_LEN 4096
#define BUFFER_SIZE 8192
#define MAX_ROUTES 50
#define MAX_ROUTE_PATH 256
#define MAX_ROUTE_RESPONSE 4096

typedef struct {
    char method[16];
    char path[MAX_ROUTE_PATH];
    char response[MAX_ROUTE_RESPONSE];
    int active;
} Route;

static Route routes[MAX_ROUTES];
static int route_count = 0;
static int server_running = 0;
extern int is_http_loaded;
extern char* trim(char *str);
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

static void sanitize_url(const char *url, char *out, size_t out_size) {
    char tmp[MAX_VALUE_LEN];
    strncpy(tmp, url, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *start_quote = strchr(tmp, '"');
    if (start_quote) {
        char *end_quote = strchr(start_quote + 1, '"');
        if (end_quote) {
            *end_quote = '\0';
            memmove(tmp, start_quote + 1, strlen(start_quote + 1) + 1);
        }
    }
    start_quote = strchr(tmp, '\'');
    if (start_quote) {
        char *end_quote = strchr(start_quote + 1, '\'');
        if (end_quote) {
            *end_quote = '\0';
            memmove(tmp, start_quote + 1, strlen(start_quote + 1) + 1);
        }
    }

    char *clean = trim(tmp);
    strncpy(out, clean, out_size - 1);
    out[out_size - 1] = '\0';
}

static void http_request(const char *method, const char *url, char *buffer, int headers_only) {
    char clean_url[MAX_VALUE_LEN];
    sanitize_url(url, clean_url, sizeof(clean_url));
    if (strcmp(method, "HEAD") == 0 || headers_only) net_http_head(clean_url, buffer);
    else net_http_get(clean_url, buffer);
}

void http_get(const char *url, char *buffer) {
    http_request("GET", url, buffer, 0);
}

void http_head(const char *url, char *buffer) {
    http_request("HEAD", url, buffer, 1);
}

void add_route(const char *method, const char *path, const char *response) {
    if (route_count < MAX_ROUTES) {
        strncpy(routes[route_count].method, method, sizeof(routes[route_count].method) - 1);
        routes[route_count].method[sizeof(routes[route_count].method) - 1] = '\0';
        strncpy(routes[route_count].path, path, MAX_ROUTE_PATH - 1);
        routes[route_count].path[MAX_ROUTE_PATH - 1] = '\0';
        strncpy(routes[route_count].response, response, MAX_ROUTE_RESPONSE - 1);
        routes[route_count].response[MAX_ROUTE_RESPONSE - 1] = '\0';
        routes[route_count].active = 1;
        route_count++;
    }
}

const char* find_route(const char *method, const char *path) {
    for (int i = 0; i < route_count; i++) {
        if (routes[i].active &&
            strcasecmp(routes[i].method, method) == 0 &&
            strcmp(routes[i].path, path) == 0) {
            return routes[i].response;
        }
    }
    return NULL;
}

void* handle_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_fd);
        return NULL;
    }
    buffer[bytes] = '\0';
    
    char method[16] = "";
    char path[256] = "";
    char version[16] = "";
    sscanf(buffer, "%15s %255s %15s", method, path, version);
    
    int found = 1;
    const char *response_body = find_route(method, path);
    if (!response_body) {
        response_body = "<h1>404 Not Found</h1><p>The requested path was not found.</p>";
        found = 0;
    }

    const char *content_type = "text/html";
    if (response_body[0] == '{' || response_body[0] == '[') {
        content_type = "application/json";
    }
    
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             found ? "200 OK" : "404 Not Found",
             content_type,
             strlen(response_body), response_body);
    
    send(client_fd, response, strlen(response), 0);
    close(client_fd);
    return NULL;
}

void http_server_start(int port) {
    if (server_running) {
        printf("Server already running on port %d\n", port);
        return;
    }
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Failed to create socket\n");
        return;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("Failed to set socket options\n");
        close(server_fd);
        return;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Failed to bind to port %d\n", port);
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 10) < 0) {
        printf("Failed to listen on port %d\n", port);
        close(server_fd);
        return;
    }
    
    server_running = 1;
    printf("HTTP Server started on port %d\n", port);
    printf("Routes registered: %d\n", route_count);
    
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_fd);
        pthread_detach(thread);
    }
    
    close(server_fd);
}

void parse_route_definition(const char *line) {
    char temp[MAX_VALUE_LEN];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *method_start = strstr(temp, "http.");
    if (!method_start) return;
    
    method_start += 5;
    char *paren = strchr(method_start, '(');
    if (!paren) return;
    *paren = '\0';

    char method[16];
    strncpy(method, method_start, sizeof(method) - 1);
    method[sizeof(method) - 1] = '\0';
    for (int i = 0; method[i]; i++) {
        method[i] = (char)toupper((unsigned char)method[i]);
    }
    
    char *path = trim(paren + 1);
    char *close_paren = strchr(path, ')');
    if (!close_paren) return;
    *close_paren = '\0';
    path = trim(path);
    
    int plen = (int)strlen(path);
    if (plen >= 2 && ((path[0] == '"' && path[plen - 1] == '"') ||
                      (path[0] == '\'' && path[plen - 1] == '\''))) {
        path[plen - 1] = '\0';
        path++;
    }
    
    char *response_expr = trim(close_paren + 1);
    char response[MAX_VALUE_LEN];
    evaluate_value(response_expr, response);
    add_route(method, path, response);
    printf("Route registered: %s %s\n", method, path);
}

static int is_route_line(const char *line) {
    const char *paren = strchr(line, '(');
    if (!paren) return 0;
    const char *close = strchr(paren, ')');
    if (!close) return 0;
    const char *after = close + 1;
    while (isspace((unsigned char)*after)) after++;
    return (*after == '"' || *after == '\'');
}

int validate_http_command(const char *line) {
    char temp[MAX_VALUE_LEN];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *func_start = strstr(temp, "http.");
    if (!func_start) return 0;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 5;
        if (strcmp(func_name, "server") == 0) {
            return 1;
        }
        return 0;
    }
    
    *paren = '\0';
    char *func_name = func_start + 5;
    char *args = paren + 1;
    
    char *close_paren = strchr(args, ')');
    if (!close_paren) return 0;
    *close_paren = '\0';
    
    if (strcmp(func_name, "get") == 0 || strcmp(func_name, "head") == 0) {
        if (strlen(args) > 0) return 1;
    }
    else if (strcmp(func_name, "post") == 0 || strcmp(func_name, "put") == 0 ||
             strcmp(func_name, "delete") == 0) {
        if (strlen(args) > 0) return 1;
    }
    else if (strcmp(func_name, "server") == 0) {
        if (strlen(args) > 0) return 1;
    }
    
    return 0;
}

void execute_http_command(char *line) {
    if (is_route_line(line)) {
        parse_route_definition(line);
        return;
    }

    char temp[MAX_VALUE_LEN];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *func_start = strstr(temp, "http.");
    if (!func_start) return;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 5;
        if (strcmp(func_name, "server") == 0) {
            http_server_start(8080);
        }
        return;
    }
    
    *paren = '\0';
    char *func_name = func_start + 5;
    char *args = paren + 1;
    char *close_paren = strchr(args, ')');
    if (!close_paren) return;
    *close_paren = '\0';
    
    args = trim(args);
    char result[MAX_VALUE_LEN];
    
    if (strcmp(func_name, "get") == 0) {
        http_get(args, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "head") == 0) {
        http_head(args, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "server") == 0) {
        int port = atoi(args);
        if (port == 0) port = 8080;
        http_server_start(port);
    }
    else if (strcmp(func_name, "post") == 0 || strcmp(func_name, "put") == 0 ||
             strcmp(func_name, "delete") == 0) {
        parse_route_definition(line);
    }
}

int resolve_http_call(const char *token, char *output_buffer, char *arg) {
    char func[64];
    extract_func_name(token, func, sizeof(func));

    if (strcmp(func, "http.get") == 0 && arg) {
        http_get(arg, output_buffer);
        return 1;
    }
    else if (strcmp(func, "http.head") == 0 && arg) {
        http_head(arg, output_buffer);
        return 1;
    }
    return 0;
}

void load_http_library() {
    if (!is_http_loaded) {
        is_http_loaded = 1;
        route_count = 0;
        server_running = 0;
        /* libcurl is initialized lazily by the net library. */
    }
}
