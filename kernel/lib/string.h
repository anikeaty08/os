/*
 * AstraOS - String Library Header
 * Standard string and memory functions for kernel use
 */

#ifndef _ASTRA_STRING_H
#define _ASTRA_STRING_H

#include <stddef.h>
#include <stdint.h>

/* Memory functions */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* String functions */
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *str, const char *delim);

/* Case-insensitive comparison */
int strcasecmp(const char *s1, const char *s2);

#endif /* _ASTRA_STRING_H */
