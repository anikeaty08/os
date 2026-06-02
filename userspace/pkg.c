#include "syscall.h"

int main(void) {
    const char msg[] =
        "AstraOS package status\n"
        "installed built-in apps:\n"
        "  /SH\n"
        "  /HELLO\n"
        "packages: built-in image only\n"
        "updates: not configured\n";

    sys_write(1, msg, sizeof(msg) - 1);
    return 0;
}
