/*
 * AstraOS - Shell Commands Header
 * Built-in command implementations
 */

#ifndef _ASTRA_SHELL_COMMANDS_H
#define _ASTRA_SHELL_COMMANDS_H

/*
 * Command function type
 */
typedef void (*cmd_func_t)(int argc, char **argv);

/*
 * Built-in commands
 */
void cmd_help(int argc, char **argv);
void cmd_clear(int argc, char **argv);
void cmd_echo(int argc, char **argv);
void cmd_mem(int argc, char **argv);
void cmd_uptime(int argc, char **argv);
void cmd_cpuinfo(int argc, char **argv);
void cmd_reboot(int argc, char **argv);
void cmd_shutdown(int argc, char **argv);
void cmd_version(int argc, char **argv);
void cmd_test(int argc, char **argv);
void cmd_ls(int argc, char **argv);
void cmd_cat(int argc, char **argv);
void cmd_ps(int argc, char **argv);
void cmd_aniket(int argc, char **argv);

/* Phase 1: New AstraOS Commands */
void cmd_status(int argc, char **argv);
void cmd_theme(int argc, char **argv);
void cmd_explore(int argc, char **argv);
void cmd_view(int argc, char **argv);

#endif /* _ASTRA_SHELL_COMMANDS_H */
