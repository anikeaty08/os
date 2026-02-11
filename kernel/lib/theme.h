#ifndef THEME_H
#define THEME_H

#include <stdint.h>

/*
 * AstraOS Color Theme System
 * Provides multiple professional color schemes
 */

/* ANSI Color Codes */
#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_UNDERLINE   "\033[4m"
#define ANSI_BLINK       "\033[5m"
#define ANSI_REVERSE     "\033[7m"

/* Foreground Colors */
#define ANSI_FG_BLACK    "\033[30m"
#define ANSI_FG_RED      "\033[31m"
#define ANSI_FG_GREEN    "\033[32m"
#define ANSI_FG_YELLOW   "\033[33m"
#define ANSI_FG_BLUE     "\033[34m"
#define ANSI_FG_MAGENTA  "\033[35m"
#define ANSI_FG_CYAN     "\033[36m"
#define ANSI_FG_WHITE    "\033[37m"

/* Bright Foreground Colors */
#define ANSI_FG_BRIGHT_BLACK   "\033[90m"
#define ANSI_FG_BRIGHT_RED     "\033[91m"
#define ANSI_FG_BRIGHT_GREEN   "\033[92m"
#define ANSI_FG_BRIGHT_YELLOW  "\033[93m"
#define ANSI_FG_BRIGHT_BLUE    "\033[94m"
#define ANSI_FG_BRIGHT_MAGENTA "\033[95m"
#define ANSI_FG_BRIGHT_CYAN    "\033[96m"
#define ANSI_FG_BRIGHT_WHITE   "\033[97m"

/* Background Colors */
#define ANSI_BG_BLACK    "\033[40m"
#define ANSI_BG_RED      "\033[41m"
#define ANSI_BG_GREEN    "\033[42m"
#define ANSI_BG_YELLOW   "\033[43m"
#define ANSI_BG_BLUE     "\033[44m"
#define ANSI_BG_MAGENTA  "\033[45m"
#define ANSI_BG_CYAN     "\033[46m"
#define ANSI_BG_WHITE    "\033[47m"

/* Theme Structure */
typedef struct {
    const char *name;
    const char *prompt_user;
    const char *prompt_host;
    const char *prompt_dir;
    const char *prompt_symbol;
    const char *success;
    const char *warning;
    const char *error;
    const char *info;
    const char *accent1;
    const char *accent2;
} ColorTheme;

/* Available Themes */
typedef enum {
    THEME_ASTRA_DARK = 0,
    THEME_NEON,
    THEME_OCEAN,
    THEME_FOREST,
    THEME_ASTRA_LIGHT,
    THEME_COUNT
} ThemeID;

/* Theme Functions */
void theme_init(void);
void theme_set(ThemeID id);
ThemeID theme_get_current(void);
const ColorTheme* theme_get_active(void);
const char* theme_get_name(ThemeID id);

/* Convenience macros for colored output */
#define COLOR_SUCCESS  (theme_get_active()->success)
#define COLOR_WARNING  (theme_get_active()->warning)
#define COLOR_ERROR    (theme_get_active()->error)
#define COLOR_INFO     (theme_get_active()->info)
#define COLOR_ACCENT1  (theme_get_active()->accent1)
#define COLOR_ACCENT2  (theme_get_active()->accent2)

#endif /* THEME_H */
