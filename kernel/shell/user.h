#ifndef USER_H
#define USER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * AstraOS User Management System
 * Multi-user support with password authentication
 */

#define MAX_USERS 16
#define MAX_USERNAME_LEN 32
#define MAX_PASSWORD_LEN 64

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password_hash[MAX_PASSWORD_LEN];  /* Simple hash for now */
    bool is_admin;
    bool is_active;
    uint64_t created_time;
    uint64_t last_login;
} User;

/* User management functions */
void user_system_init(void);
bool user_create(const char *username, const char *password, bool is_admin);
bool user_authenticate(const char *username, const char *password);
bool user_exists(const char *username);
bool user_delete(const char *username);
User* user_get_current(void);
const char* user_get_current_name(void);
bool user_is_admin(void);

/* Password verification for sensitive operations */
bool user_verify_password(const char *password);

/* Simple hash function (for demonstration - use better crypto in production) */
void hash_password(const char *password, char *hash_out);

#endif /* USER_H */
