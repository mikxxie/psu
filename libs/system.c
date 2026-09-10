#include <stdio.h>
#include <string.h>
#include <stdlib.h>  // ADD THIS
#include <time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include "system.h"

#define MAX_VALUE_LEN 4096

void get_system_time(char *buffer) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, MAX_VALUE_LEN, "%H:%M:%S", timeinfo);
}

void get_system_date(char *buffer) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, MAX_VALUE_LEN, "%Y-%m-%d", timeinfo);
}

void get_system_os(char *buffer) {
    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        snprintf(buffer, MAX_VALUE_LEN, "%s %s (%s)", 
                 uname_data.sysname, uname_data.release, uname_data.machine);
    } else {
        strcpy(buffer, "Unknown OS");
    }
}

void get_system_hostname(char *buffer) {
    if (gethostname(buffer, MAX_VALUE_LEN) != 0) {
        strcpy(buffer, "Unknown");
    }
}

void get_system_username(char *buffer) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strcpy(buffer, pw->pw_name);
    } else {
        strcpy(buffer, getenv("USER") ? getenv("USER") : "Unknown");
    }
}

void get_system_ip4(char *buffer) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        strcpy(buffer, "null");
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            if (strcmp(ip, "127.0.0.1") != 0) {
                strcpy(buffer, ip);
                found = 1;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    if (!found) strcpy(buffer, "null");
}

void get_system_ip6(char *buffer) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        strcpy(buffer, "null");
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ifa->ifa_addr;
            char ip[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr->sin6_addr, ip, INET6_ADDRSTRLEN);
            if (strcmp(ip, "::1") != 0) {
                strcpy(buffer, ip);
                found = 1;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    if (!found) strcpy(buffer, "null");
}

int resolve_system_call(const char *token, char *output_buffer) {
    if (strcmp(token, "system.time") == 0) {
        get_system_time(output_buffer);
        return 1;
    } 
    else if (strcmp(token, "system.date") == 0) {
        get_system_date(output_buffer);
        return 1;
    } 
    else if (strcmp(token, "system.os") == 0) {
        get_system_os(output_buffer);
        return 1;
    }
    else if (strcmp(token, "system.hostname") == 0) {
        get_system_hostname(output_buffer);
        return 1;
    }
    else if (strcmp(token, "system.username") == 0) {
        get_system_username(output_buffer);
        return 1;
    }
    else if (strcmp(token, "system.ip4") == 0) {
        get_system_ip4(output_buffer);
        return 1;
    }
    else if (strcmp(token, "system.ip6") == 0) {
        get_system_ip6(output_buffer);
        return 1;
    }
    return 0;
}
