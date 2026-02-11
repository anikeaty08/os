#ifndef LOGIN_H
#define LOGIN_H

#include <stdbool.h>

/*
 * AstraOS Login System
 */

/* Login credentials */
#define LOGIN_USERNAME "aniket"
#define LOGIN_PASSWORD "astra"

/* Login functions */
bool login_prompt(void);
void login_show_welcome(void);

#endif /* LOGIN_H */
