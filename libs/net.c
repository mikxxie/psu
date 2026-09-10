#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <curl/curl.h>
#include "net.h"

#define MAX_VALUE_LEN 4096

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} NetCurlBuffer;

static int curl_ready = 0;

static int ensure_curl(void) {
    if (!curl_ready) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 0;
        curl_ready = 1;
    }
    return 1;
}

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    NetCurlBuffer *out = (NetCurlBuffer *)userdata;
    size_t count = size * nmemb;
    size_t room = out->cap > out->len + 1 ? out->cap - out->len - 1 : 0;
    size_t copy = count < room ? count : room;
    if (copy) {
        memcpy(out->buf + out->len, ptr, copy);
        out->len += copy;
        out->buf[out->len] = '\0';
    }
    return count;
}

static void net_http_request(const char *method, const char *url, char *buffer, int headers_only) {
    buffer[0] = '\0';
    if (!url || !*url) {
        strcpy(buffer, "Error: empty URL");
        return;
    }
    if (!ensure_curl()) {
        strcpy(buffer, "Error: HTTP library init failed");
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        strcpy(buffer, "Error: HTTP request failed");
        return;
    }
    NetCurlBuffer out = { buffer, MAX_VALUE_LEN, 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PSU/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    if (headers_only || strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        snprintf(buffer, MAX_VALUE_LEN, "Error: HTTP request failed (%s)", curl_easy_strerror(rc));
    } else if (out.len == 0) {
        strcpy(buffer, headers_only ? "Empty headers" : "Empty response");
    } else {
        while (out.len && (buffer[out.len - 1] == '\n' || buffer[out.len - 1] == '\r'))
            buffer[--out.len] = '\0';
    }
    curl_easy_cleanup(curl);
}

void net_http_get(const char *url, char *buffer) { net_http_request("GET", url, buffer, 0); }
void net_http_head(const char *url, char *buffer) { net_http_request("HEAD", url, buffer, 1); }

extern int is_net_loaded;
extern char* trim(char *str);
extern void get_system_ip4(char *buffer);
extern void get_system_ip6(char *buffer);

void net_ping(const char *host, char *buffer) {
    char command[512];
    char host_copy[MAX_VALUE_LEN];
    strcpy(host_copy, host);
    
    // Remove quotes if present
    if (host_copy[0] == '"' && host_copy[strlen(host_copy)-1] == '"') {
        host_copy[strlen(host_copy)-1] = '\0';
        memmove(host_copy, host_copy + 1, strlen(host_copy));
    }
    if (host_copy[0] == '\'' && host_copy[strlen(host_copy)-1] == '\'') {
        host_copy[strlen(host_copy)-1] = '\0';
        memmove(host_copy, host_copy + 1, strlen(host_copy));
    }
    
    snprintf(command, sizeof(command), "ping -c 1 -W 1 %s 2>&1 | head -n 2", host_copy);
    FILE *fp = popen(command, "r");
    if (fp) {
        char result[MAX_VALUE_LEN] = "";
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            strcat(result, line);
        }
        pclose(fp);
        if (strlen(result) > 0) {
            // Remove newline
            char *newline = strchr(result, '\n');
            if (newline) *newline = '\0';
            strcpy(buffer, result);
        } else {
            strcpy(buffer, "Host unreachable");
        }
    } else {
        strcpy(buffer, "Ping failed");
    }
}

void net_dns_lookup(const char *host, char *buffer) {
    struct hostent *he;
    struct in_addr **addr_list;
    char host_copy[MAX_VALUE_LEN];
    strcpy(host_copy, host);
    
    // Remove quotes if present
    if (host_copy[0] == '"' && host_copy[strlen(host_copy)-1] == '"') {
        host_copy[strlen(host_copy)-1] = '\0';
        memmove(host_copy, host_copy + 1, strlen(host_copy));
    }
    if (host_copy[0] == '\'' && host_copy[strlen(host_copy)-1] == '\'') {
        host_copy[strlen(host_copy)-1] = '\0';
        memmove(host_copy, host_copy + 1, strlen(host_copy));
    }
    
    he = gethostbyname(host_copy);
    if (he == NULL) {
        strcpy(buffer, "DNS lookup failed");
        return;
    }
    
    addr_list = (struct in_addr **)he->h_addr_list;
    char result[MAX_VALUE_LEN] = "";
    for(int i = 0; addr_list[i] != NULL; i++) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, addr_list[i], ip, INET_ADDRSTRLEN);
        strcat(result, ip);
        if (addr_list[i+1] != NULL) strcat(result, ", ");
    }
    strcpy(buffer, result);
}

void net_iface_list(char *buffer) {
    struct ifaddrs *ifaddr, *ifa;
    char result[MAX_VALUE_LEN] = "";
    
    if (getifaddrs(&ifaddr) == -1) {
        strcpy(buffer, "Cannot get interfaces");
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            if (strcmp(ip, "127.0.0.1") != 0) {
                strcat(result, ifa->ifa_name);
                strcat(result, ": ");
                strcat(result, ip);
                strcat(result, "  ");
            }
        }
    }
    freeifaddrs(ifaddr);
    
    if (strlen(result) > 0) {
        // Remove trailing spaces
        char *end = result + strlen(result) - 1;
        while (end > result && isspace((unsigned char)*end)) end--;
        end[1] = '\0';
        strcpy(buffer, result);
    } else {
        strcpy(buffer, "No interfaces found");
    }
}

void net_mac_addr(const char *iface, char *buffer) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;
    char iface_copy[MAX_VALUE_LEN];
    strcpy(iface_copy, iface);
    
    // Remove quotes if present
    if (iface_copy[0] == '"' && iface_copy[strlen(iface_copy)-1] == '"') {
        iface_copy[strlen(iface_copy)-1] = '\0';
        memmove(iface_copy, iface_copy + 1, strlen(iface_copy));
    }
    if (iface_copy[0] == '\'' && iface_copy[strlen(iface_copy)-1] == '\'') {
        iface_copy[strlen(iface_copy)-1] = '\0';
        memmove(iface_copy, iface_copy + 1, strlen(iface_copy));
    }
    
    if (getifaddrs(&ifaddr) == -1) {
        strcpy(buffer, "Cannot get MAC");
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_PACKET && 
            strcmp(ifa->ifa_name, iface_copy) == 0) {
            struct sockaddr_ll *s = (struct sockaddr_ll*)ifa->ifa_addr;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     s->sll_addr[0], s->sll_addr[1], s->sll_addr[2],
                     s->sll_addr[3], s->sll_addr[4], s->sll_addr[5]);
            strcpy(buffer, mac);
            found = 1;
            break;
        }
    }
    freeifaddrs(ifaddr);
    if (!found) strcpy(buffer, "Interface not found");
}

int validate_net_command(const char *line) {
    char temp[MAX_VALUE_LEN];
    strcpy(temp, line);
    
    char *func_start = strstr(temp, "net.");
    if (!func_start) return 0;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 4;
        if (strcmp(func_name, "ip4") == 0 || strcmp(func_name, "ip6") == 0 || 
            strcmp(func_name, "ifaces") == 0) {
            return 1;
        }
        return 0;
    }
    
    *paren = '\0';
    char *func_name = func_start + 4;
    char *args = paren + 1;
    
    char *close_paren = strchr(args, ')');
    if (!close_paren) return 0;
    *close_paren = '\0';
    
    if (strcmp(func_name, "ping") == 0 || strcmp(func_name, "dns") == 0) {
        if (strlen(args) > 0) return 1;
    }
    else if (strcmp(func_name, "mac") == 0) {
        if (strlen(args) > 0) return 1;
    }
    
    return 0;
}

void execute_net_command(char *line) {
    char temp[MAX_VALUE_LEN];
    strcpy(temp, line);
    
    char *func_start = strstr(temp, "net.");
    if (!func_start) return;
    
    char *paren = strchr(func_start, '(');
    if (!paren) {
        char *func_name = func_start + 4;
        char result[MAX_VALUE_LEN];
        
        if (strcmp(func_name, "ip4") == 0) {
            get_system_ip4(result);
            printf("%s\n", result);
        } else if (strcmp(func_name, "ip6") == 0) {
            get_system_ip6(result);
            printf("%s\n", result);
        } else if (strcmp(func_name, "ifaces") == 0) {
            net_iface_list(result);
            printf("%s\n", result);
        }
        return;
    }
    
    *paren = '\0';
    char *func_name = func_start + 4;
    char *args = paren + 1;
    char *close_paren = strchr(args, ')');
    if (!close_paren) return;
    *close_paren = '\0';
    
    args = trim(args);
    char result[MAX_VALUE_LEN];
    
    if (strcmp(func_name, "ping") == 0) {
        net_ping(args, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "dns") == 0) {
        net_dns_lookup(args, result);
        printf("%s\n", result);
    }
    else if (strcmp(func_name, "mac") == 0) {
        net_mac_addr(args, result);
        printf("%s\n", result);
    }
}

int resolve_net_call(const char *token, char *output_buffer, char *arg) {
    char func[64];
    const char *paren = strchr(token, '(');
    size_t n = paren ? (size_t)(paren - token) : strlen(token);
    if (n >= sizeof(func)) n = sizeof(func) - 1;
    memcpy(func, token, n);
    func[n] = '\0';
    while (n > 0 && isspace((unsigned char)func[n - 1])) {
        func[--n] = '\0';
    }

    if (strcmp(func, "net.ip4") == 0) {
        get_system_ip4(output_buffer);
        return 1;
    }
    else if (strcmp(func, "net.ip6") == 0) {
        get_system_ip6(output_buffer);
        return 1;
    }
    else if (strcmp(func, "net.ifaces") == 0) {
        net_iface_list(output_buffer);
        return 1;
    }
    else if (strcmp(func, "net.ping") == 0 && arg) {
        net_ping(arg, output_buffer);
        return 1;
    }
    else if (strcmp(func, "net.dns") == 0 && arg) {
        net_dns_lookup(arg, output_buffer);
        return 1;
    }
    else if (strcmp(func, "net.mac") == 0 && arg) {
        net_mac_addr(arg, output_buffer);
        return 1;
    }
    return 0;
}

void load_net_library() {
    if (!is_net_loaded) {
        is_net_loaded = 1;
    }
}
