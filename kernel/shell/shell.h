/*
 * AstraOS - Shell Header
 * Interactive command interpreter
 */

#ifndef _ASTRA_SHELL_SHELL_H
#define _ASTRA_SHELL_SHELL_H

/*
 * Initialize and run the shell
 * This function does not return
 */
void shell_run(void);

/*
 * Process a single command line
 */
void shell_execute(const char *line);

#endif /* _ASTRA_SHELL_SHELL_H */
