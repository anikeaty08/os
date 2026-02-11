/*
 * AstraOS - User Management System
 * Multi-user support with password authentication
 */

#include "user.h"
#include "../lib/string.h"
#include "../lib/stdio.h"
#include "../drivers/pit.h"

static User users[MAX_USERS];
static int user_count = 0;
static User *current_user = NULL;

/* Simple hash function (XOR-based, for demonstration only) */
void hash_password(const char *password, char *hash_out) {
    uint32_t hash = 0x5A5A5A5A;  /* Seed */
    
    for (int i = 0; password[i]; i++) {
        hash ^= (uint32_t)password[i];
        hash = (hash << 5) | (hash >> 27);  /* Rotate left 5 bits */
        hash ^= (hash >> 16);
    }
    
    /* Convert to hex string */
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        hash_out[i*2] = hex[(hash >> (28 - i*4)) & 0xF];
        hash_out[i*2 + 1] = hex[(hash >> (24 - i*4)) & 0xF];
    }
    hash_out[16] = '\0';
}

void user_system_init(void) {
    user_count = 0;
    current_user = NULL;
    
    /* Create default admin user */
    user_create("aniket", "astra", true);
}

bool user_create(const char *username, const char *password, bool is_admin) {
    /* Check if user already exists */
    if (user_exists(username)) {
        return false;
    }
    
    /* Check if we have space */
    if (user_count >= MAX_USERS) {
        return false;
    }
    
    /* Validate username */
    if (strlen(username) == 0 || strlen(username) >= MAX_USERNAME_LEN) {
        return false;
    }
    
    /* Validate password */
    if (strlen(password) < 4 || strlen(password) >= MAX_PASSWORD_LEN) {
        return false;
    }
    
    /* Create user */
    User *user = &users[user_count++];
    strncpy(user->username, username, MAX_USERNAME_LEN - 1);
    user->username[MAX_USERNAME_LEN - 1] = '\0';
    
    hash_password(password, user->password_hash);
    user->is_admin = is_admin;
    user->is_active = true;
    user->created_time = pit_get_ticks();
    user->last_login = 0;
    
    return true;
}

bool user_authenticate(const char *username, const char *password) {
    char hash[MAX_PASSWORD_LEN];
    hash_password(password, hash);
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].is_active && strcmp(users[i].username, username) == 0) {
            if (strcmp(users[i].password_hash, hash) == 0) {
                current_user = &users[i];
                current_user->last_login = pit_get_ticks();
                return true;
            }
        }
    }
    
    return false;
}

bool user_exists(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return true;
        }
    }
    return false;
}

bool user_delete(const char *username) {
    /* Only admin can delete users */
    if (!user_is_admin()) {
        return false;
    }
    
    /* Cannot delete yourself */
    if (current_user && strcmp(current_user->username, username) == 0) {
        return false;
    }
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].is_active = false;
            return true;
        }
    }
    
    return false;
}

User* user_get_current(void) {
    return current_user;
}

const char* user_get_current_name(void) {
    return current_user ? current_user->username : "guest";
}

bool user_is_admin(void) {
    return current_user && current_user->is_admin;
}

bool user_verify_password(const char *password) {
    if (!current_user) return false;
    
    char hash[MAX_PASSWORD_LEN];
    hash_password(password, hash);
    
    return strcmp(current_user->password_hash, hash) == 0;
}
