#include "syscall.h"

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static void puts(const char *s) {
    sys_write(1, s, strlen(s));
}

static void trim_newline(char *s) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            return;
        }
    }
}

static void print_pid(void) {
    long pid = sys_getpid();
    char buf[32];
    int pos = 0;

    if (pid == 0) {
        puts("0\n");
        return;
    }

    char tmp[24];
    while (pid > 0 && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (pid % 10));
        pid /= 10;
    }

    int out = 0;
    while (pos > 0) {
        buf[out++] = tmp[--pos];
    }
    buf[out++] = '\n';
    sys_write(1, buf, (size_t)out);
}

int main(void) {
    char line[128];

    puts("AstraOS user shell\n");
    puts("type help for commands\n");

    for (;;) {
        puts("user$ ");

        long n = sys_read(0, line, sizeof(line) - 1);
        if (n <= 0) {
            continue;
        }

        line[n] = '\0';
        trim_newline(line);

        if (strcmp(line, "help") == 0) {
            puts("commands: help pid echo exit yield\n");
        } else if (strcmp(line, "pid") == 0) {
            print_pid();
        } else if (strcmp(line, "yield") == 0) {
            sys_yield();
        } else if (strcmp(line, "echo") == 0) {
            puts("echo from ring 3\n");
        } else if (strcmp(line, "exit") == 0) {
            puts("leaving user shell\n");
            return 0;
        } else if (line[0] == '\0') {
            continue;
        } else {
            puts("unknown command\n");
        }
    }
}
