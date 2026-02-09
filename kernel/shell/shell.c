/*
 * AstraOS - Shell Implementation
 * Interactive command interpreter
 */

#include "shell.h"
#include "commands.h"
#include "../lib/string.h"
#include "../lib/stdio.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"

/*
 * Command buffer
 */
#define CMD_BUFFER_SIZE 256
#define MAX_ARGS 16

static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_pos = 0;

/*
 * Command history (simple implementation)
 */
#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][CMD_BUFFER_SIZE];
static int history_count = 0;
static int history_pos = 0;

/*
 * Add command to history
 */
static void history_add(const char *cmd) {
    if (strlen(cmd) == 0) return;

    /* Don't add duplicates of last command */
    if (history_count > 0 && strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return;
    }

    strcpy(history[history_count % HISTORY_SIZE], cmd);
    history_count++;
    history_pos = history_count;
}

/*
 * Parse command line into arguments
 */
static int parse_args(char *line, char **argv) {
    int argc = 0;
    char *token = strtok(line, " \t");

    while (token && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    argv[argc] = NULL;
    return argc;
}

/*
 * Execute command line
 */
void shell_execute(const char *line) {
    /* Copy line since strtok modifies it */
    char line_copy[CMD_BUFFER_SIZE];
    strncpy(line_copy, line, CMD_BUFFER_SIZE - 1);
    line_copy[CMD_BUFFER_SIZE - 1] = '\0';

    /* Parse arguments */
    char *argv[MAX_ARGS];
    int argc = parse_args(line_copy, argv);

    if (argc == 0) return;

    /* Find and execute command */
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0) {
        cmd_help(argc, argv);
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        cmd_clear(argc, argv);
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(argc, argv);
    } else if (strcmp(cmd, "mem") == 0 || strcmp(cmd, "memory") == 0) {
        cmd_mem(argc, argv);
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime(argc, argv);
    } else if (strcmp(cmd, "cpuinfo") == 0) {
        cmd_cpuinfo(argc, argv);
    } else if (strcmp(cmd, "reboot") == 0) {
        cmd_reboot(argc, argv);
    } else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "halt") == 0) {
        cmd_shutdown(argc, argv);
    } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "ver") == 0) {
        cmd_version(argc, argv);
    } else if (strcmp(cmd, "test") == 0) {
        cmd_test(argc, argv);
    } else {
        kprintf("Unknown command: %s\n", cmd);
        kprintf("Type 'help' for available commands.\n");
    }
}

/*
 * Print shell prompt
 */
static void print_prompt(void) {
    kprintf("\nAstraOS> ");
}

/*
 * Main shell loop
 */
void shell_run(void) {
    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  Welcome to AstraOS Shell\n");
    kprintf("  Type 'help' for available commands\n");
    kprintf("========================================\n");

    print_prompt();
    cmd_pos = 0;
    cmd_buffer[0] = '\0';

    while (1) {
        /* Check for rescheduling (cooperative multitasking point) */
        if (pit_check_reschedule()) {
            pit_clear_reschedule();
            /* Would call schedule() here if we had processes */
        }

        /* Get character from keyboard */
        if (!keyboard_has_key()) {
            __asm__ volatile ("hlt");
            continue;
        }

        char c = keyboard_getchar();

        if (c == '\n') {
            /* Execute command */
            kprintf("\n");
            cmd_buffer[cmd_pos] = '\0';

            if (cmd_pos > 0) {
                history_add(cmd_buffer);
                shell_execute(cmd_buffer);
            }

            print_prompt();
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
        } else if (c == '\b') {
            /* Backspace */
            if (cmd_pos > 0) {
                cmd_pos--;
                cmd_buffer[cmd_pos] = '\0';
                kprintf("\b \b");  /* Erase character on screen */
            }
        } else if (c == '\t') {
            /* Tab - could implement completion here */
            /* For now, just insert spaces */
            if (cmd_pos < CMD_BUFFER_SIZE - 4) {
                for (int i = 0; i < 4 && cmd_pos < CMD_BUFFER_SIZE - 1; i++) {
                    cmd_buffer[cmd_pos++] = ' ';
                    kprintf(" ");
                }
            }
        } else if (c >= 32 && c < 127) {
            /* Printable character */
            if (cmd_pos < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_pos++] = c;
                cmd_buffer[cmd_pos] = '\0';
                kprintf("%c", c);
            }
        }
    }
}
