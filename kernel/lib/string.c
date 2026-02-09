/*
 * AstraOS - String Library Implementation
 * Standard string and memory functions for kernel use
 */

#include "string.h"

/*
 * memcpy - Copy memory area
 * Required by GCC for struct assignments
 */
void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

/*
 * memset - Fill memory with constant byte
 * Required by GCC for struct initialization
 */
void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    while (n--) {
        *p++ = (uint8_t)c;
    }

    return s;
}

/*
 * memmove - Copy memory area (handles overlapping)
 */
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        /* Copy forward */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Copy backward */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

/*
 * memcmp - Compare memory areas
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

/*
 * strlen - Calculate string length
 */
size_t strlen(const char *s) {
    size_t len = 0;

    while (*s++) {
        len++;
    }

    return len;
}

/*
 * strcpy - Copy string
 */
char *strcpy(char *dest, const char *src) {
    char *d = dest;

    while ((*d++ = *src++));

    return dest;
}

/*
 * strncpy - Copy string with length limit
 */
char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;

    while (n && (*d++ = *src++)) {
        n--;
    }

    /* Pad with zeros if needed */
    while (n--) {
        *d++ = '\0';
    }

    return dest;
}

/*
 * strcmp - Compare strings
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/*
 * strncmp - Compare strings with length limit
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;

    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/*
 * strchr - Locate character in string
 */
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }

    return (c == '\0') ? (char *)s : NULL;
}

/*
 * strrchr - Locate last occurrence of character in string
 */
char *strrchr(const char *s, int c) {
    const char *last = NULL;

    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }

    return (c == '\0') ? (char *)s : (char *)last;
}

/*
 * strstr - Locate substring
 */
char *strstr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);

    if (needle_len == 0) {
        return (char *)haystack;
    }

    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }

    return NULL;
}

/*
 * strtok - Tokenize string
 */
static char *strtok_save = NULL;

char *strtok(char *str, const char *delim) {
    char *start;
    char *end;

    if (str) {
        strtok_save = str;
    }

    if (!strtok_save || !*strtok_save) {
        return NULL;
    }

    /* Skip leading delimiters */
    start = strtok_save;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (!*start) {
        strtok_save = NULL;
        return NULL;
    }

    /* Find end of token */
    end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        strtok_save = end + 1;
    } else {
        strtok_save = NULL;
    }

    return start;
}

/*
 * strcasecmp - Case-insensitive string comparison
 */
static inline char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) {
        s1++;
        s2++;
    }

    return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}
