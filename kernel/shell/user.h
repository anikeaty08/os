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
#define MAX_PASSWORD_LEN 65

#define USER_RECORDS_PATH "/USERS.TXT"
#define USER_RECORDS_MAX_BYTES 2048

/*
 * Persistent user record format, one record per line:
 * username:password_hash:uid:is_admin:is_active:created_time:last_login
 *
 * password_hash is the 64-character SHA-256 hex output from hash_password().
 * Legacy records without uid and legacy 16-character demo hashes are accepted.
 * Boolean fields are 0 or 1. Lines beginning with # are ignored.
 */

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password_hash[MAX_PASSWORD_LEN];
    uint32_t uid;
    bool is_admin;
    bool is_active;
    uint64_t created_time;
    uint64_t last_login;
} User;

/* User management functions */
void user_system_init(void);
int user_count_users(void);
bool user_create(const char *username, const char *password, bool is_admin);
bool user_authenticate(const char *username, const char *password);
bool user_exists(const char *username);
bool user_delete(const char *username);
User* user_get_current(void);
const User* user_get_by_index(int index);
const char* user_get_current_name(void);
bool user_is_admin(void);
uint32_t user_get_current_uid(void);

/* Password verification for sensitive operations */
bool user_verify_password(const char *password);

/* SHA-256 password hash helper */
void hash_password(const char *password, char *hash_out);

#endif /* USER_H */
