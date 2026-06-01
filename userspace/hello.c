#include "syscall.h"

int main(void) {
    const char msg[] = "hello from AstraOS ring 3\n";
    sys_write(1, msg, sizeof(msg) - 1);
    return 0;
}
