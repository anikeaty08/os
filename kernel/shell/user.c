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

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t rotr32(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

static uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void store_be64(uint8_t *p, uint64_t value) {
    for (int i = 7; i >= 0; i--) {
        p[7 - i] = (uint8_t)(value >> (i * 8));
    }
}

static void sha256_transform(uint32_t state[8], const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = load_be32(block + i * 4);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha256_hash(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint32_t state[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    uint8_t block[SHA256_BLOCK_SIZE];
    size_t offset = 0;

    while (len - offset >= SHA256_BLOCK_SIZE) {
        sha256_transform(state, data + offset);
        offset += SHA256_BLOCK_SIZE;
    }

    size_t rem = len - offset;
    memset(block, 0, sizeof(block));
    memcpy(block, data + offset, rem);
    block[rem] = 0x80;

    if (rem >= 56) {
        sha256_transform(state, block);
        memset(block, 0, sizeof(block));
    }

    store_be64(block + 56, (uint64_t)len * 8U);
    sha256_transform(state, block);

    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (uint8_t)(state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)state[i];
    }
}

static void legacy_hash_password(const char *password, char *hash_out) {
    uint32_t hash = 0x5A5A5A5A;

    for (int i = 0; password[i]; i++) {
        hash ^= (uint32_t)password[i];
        hash = (hash << 5) | (hash >> 27);
        hash ^= (hash >> 16);
    }

    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        hash_out[i * 2] = hex[(hash >> (28 - i * 4)) & 0xF];
        hash_out[i * 2 + 1] = hex[(hash >> (24 - i * 4)) & 0xF];
    }
    hash_out[16] = '\0';
}
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
    size_t len = strlen(hash);
    if ((len != 16 && len != 64) || len >= MAX_PASSWORD_LEN) {
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
    char *uid_field = next_user_field(&cursor);
    char *admin = next_user_field(&cursor);
    char *active = next_user_field(&cursor);
    char *created = next_user_field(&cursor);
    char *last_login = next_user_field(&cursor);
    bool has_uid = true;
    uint64_t parsed_uid = 0;
    bool is_admin = false;
    bool is_active = false;
    uint64_t created_time = 0;
    uint64_t last_login_time = 0;

    if (!username || !hash || !uid_field || !admin || !active || !created) {
        return false;
    }

    if (!last_login) {
        has_uid = false;
        last_login = created;
        created = active;
        active = admin;
        admin = uid_field;
        parsed_uid = (uint64_t)user_count;
    } else {
        if (cursor) {
            return false;
        }
        if (!parse_u64_field(uid_field, &parsed_uid) || parsed_uid > UINT32_MAX) {
            return false;
        }
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

    for (int i = 0; i < user_count; i++) {
        if (users[i].uid == (uint32_t)parsed_uid) {
            return false;
        }
    }

    User *user = &users[user_count++];
    strncpy(user->username, username, MAX_USERNAME_LEN - 1);
    user->username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(user->password_hash, hash, MAX_PASSWORD_LEN - 1);
    user->password_hash[MAX_PASSWORD_LEN - 1] = '\0';
    user->uid = (uint32_t)parsed_uid;
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

void hash_password(const char *password, char *hash_out) {
    uint8_t digest[SHA256_DIGEST_SIZE];
    const char *hex = "0123456789abcdef";

    sha256_hash((const uint8_t *)password, strlen(password), digest);

    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        hash_out[i * 2] = hex[digest[i] >> 4];
        hash_out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    hash_out[64] = '\0';
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
    user->uid = (uint32_t)(user_count - 1);
    user->is_admin = is_admin;
    user->is_active = true;
    user->created_time = pit_get_ticks();
    user->last_login = 0;
    
    return true;
}

bool user_authenticate(const char *username, const char *password) {
    char hash[MAX_PASSWORD_LEN];
    char legacy_hash[17];
    hash_password(password, hash);
    legacy_hash_password(password, legacy_hash);
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].is_active && strcmp(users[i].username, username) == 0) {
            if (strcmp(users[i].password_hash, hash) == 0 ||
                strcmp(users[i].password_hash, legacy_hash) == 0) {
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

const User* user_get_by_index(int index) {
    if (index < 0 || index >= user_count) {
        return NULL;
    }
    return &users[index];
}

const char* user_get_current_name(void) {
    return current_user ? current_user->username : "guest";
}

bool user_is_admin(void) {
    return current_user && current_user->is_admin;
}

uint32_t user_get_current_uid(void) {
    return current_user ? current_user->uid : 0;
}

bool user_verify_password(const char *password) {
    if (!current_user) return false;
    
    char hash[MAX_PASSWORD_LEN];
    char legacy_hash[17];
    hash_password(password, hash);
    legacy_hash_password(password, legacy_hash);
    
    return strcmp(current_user->password_hash, hash) == 0 ||
           strcmp(current_user->password_hash, legacy_hash) == 0;
}
