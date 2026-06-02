/*
 * AstraOS - User Management System
 * Multi-user support with password authentication
 */

#include "user.h"
#include "../lib/string.h"
#include "../lib/stdio.h"
#include "../drivers/pit.h"
#include "../fs/vfs.h"

static User users[MAX_USERS];
static int user_count = 0;
static User *current_user = NULL;
static char user_records_buffer[USER_RECORDS_MAX_BYTES];

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool is_valid_username_field(const char *username) {
    size_t len = strlen(username);

    if (len == 0 || len >= MAX_USERNAME_LEN) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        char c = username[i];
        if (c <= ' ' || c >= 127 || c == ':') {
            return false;
        }
    }

    return true;
}

static bool is_valid_password_hash(const char *hash) {
    if (strlen(hash) != 16 || strlen(hash) >= MAX_PASSWORD_LEN) {
        return false;
    }

    for (int i = 0; hash[i]; i++) {
        if (!is_hex_char(hash[i])) {
            return false;
        }
    }

    return true;
}

static bool parse_bool_field(const char *field, bool *out) {
    if (strcmp(field, "0") == 0) {
        *out = false;
        return true;
    }

    if (strcmp(field, "1") == 0) {
        *out = true;
        return true;
    }

    return false;
}

static bool parse_u64_field(const char *field, uint64_t *out) {
    uint64_t value = 0;

    if (!field || field[0] == '\0') {
        return false;
    }

    for (int i = 0; field[i]; i++) {
        if (field[i] < '0' || field[i] > '9') {
            return false;
        }

        uint64_t digit = (uint64_t)(field[i] - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }

    *out = value;
    return true;
}

static char *next_user_field(char **cursor) {
    char *field = *cursor;

    if (!field) {
        return NULL;
    }

    while (**cursor && **cursor != ':') {
        (*cursor)++;
    }

    if (**cursor == ':') {
        **cursor = '\0';
        (*cursor)++;
    } else {
        *cursor = NULL;
    }

    return field;
}

static bool user_record_exists(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return true;
        }
    }
    return false;
}

static bool user_add_persistent_record(char *line) {
    char *cursor = line;
    char *username = next_user_field(&cursor);
    char *hash = next_user_field(&cursor);
    char *admin = next_user_field(&cursor);
    char *active = next_user_field(&cursor);
    char *created = next_user_field(&cursor);
    char *last_login = next_user_field(&cursor);
    bool is_admin = false;
    bool is_active = false;
    uint64_t created_time = 0;
    uint64_t last_login_time = 0;

    if (!username || !hash || !admin || !active || !created || !last_login) {
        return false;
    }

    if (cursor) {
        return false;
    }

    if (user_count >= MAX_USERS || user_record_exists(username)) {
        return false;
    }

    if (!is_valid_username_field(username) ||
        !is_valid_password_hash(hash) ||
        !parse_bool_field(admin, &is_admin) ||
        !parse_bool_field(active, &is_active) ||
        !parse_u64_field(created, &created_time) ||
        !parse_u64_field(last_login, &last_login_time)) {
        return false;
    }

    User *user = &users[user_count++];
    strncpy(user->username, username, MAX_USERNAME_LEN - 1);
    user->username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(user->password_hash, hash, MAX_PASSWORD_LEN - 1);
    user->password_hash[MAX_PASSWORD_LEN - 1] = '\0';
    user->is_admin = is_admin;
    user->is_active = is_active;
    user->created_time = created_time;
    user->last_login = last_login_time;

    return true;
}

static int user_load_persistent_records(void) {
    struct vfs_node *node = vfs_open(USER_RECORDS_PATH);

    if (!node) {
        return 0;
    }

    if (vfs_is_directory(node)) {
        vfs_close(node);
        return 0;
    }

    uint64_t size = vfs_size(node);
    if (size >= USER_RECORDS_MAX_BYTES) {
        size = USER_RECORDS_MAX_BYTES - 1;
    }

    int bytes = vfs_read(node, 0, (size_t)size, (uint8_t *)user_records_buffer);
    vfs_close(node);

    if (bytes <= 0) {
        return 0;
    }

    user_records_buffer[bytes] = '\0';

    int loaded = 0;
    char *line = user_records_buffer;
    while (*line && user_count < MAX_USERS) {
        char *next = line;
        while (*next && *next != '\n') {
            next++;
        }

        if (*next == '\n') {
            *next = '\0';
            next++;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        if (line[0] != '\0' && line[0] != '#') {
            if (user_add_persistent_record(line)) {
                loaded++;
            }
        }

        line = next;
    }

    return loaded;
}

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

    /*
     * Load persisted records when a root VFS is available. If /USERS.TXT is
     * missing or contains no valid records, keep the existing first-time setup
     * path unchanged: login_prompt() creates the initial admin interactively.
     */
    user_load_persistent_records();
}

int user_count_users(void) {
    return user_count;
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
