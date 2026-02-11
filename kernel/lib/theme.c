/*
 * AstraOS Color Theme System Implementation
 */

#include "theme.h"
#include "stdio.h"
#include "string.h"

/* Theme Definitions */
static const ColorTheme themes[THEME_COUNT] = {
    /* ASTRA_DARK - Deep space with cyan/purple accents */
    {
        .name = "Astra Dark",
        .prompt_user = ANSI_FG_BRIGHT_CYAN,
        .prompt_host = ANSI_FG_BRIGHT_MAGENTA,
        .prompt_dir = ANSI_FG_BRIGHT_BLUE,
        .prompt_symbol = ANSI_FG_BRIGHT_GREEN,
        .success = ANSI_FG_BRIGHT_GREEN,
        .warning = ANSI_FG_BRIGHT_YELLOW,
        .error = ANSI_FG_BRIGHT_RED,
        .info = ANSI_FG_BRIGHT_CYAN,
        .accent1 = ANSI_FG_BRIGHT_MAGENTA,
        .accent2 = ANSI_FG_BRIGHT_CYAN
    },
    /* NEON - Cyberpunk pink/cyan/green */
    {
        .name = "Neon",
        .prompt_user = ANSI_FG_BRIGHT_MAGENTA,
        .prompt_host = ANSI_FG_BRIGHT_CYAN,
        .prompt_dir = ANSI_FG_BRIGHT_GREEN,
        .prompt_symbol = ANSI_FG_BRIGHT_MAGENTA,
        .success = ANSI_FG_BRIGHT_GREEN,
        .warning = ANSI_FG_BRIGHT_YELLOW,
        .error = ANSI_FG_BRIGHT_RED,
        .info = ANSI_FG_BRIGHT_CYAN,
        .accent1 = ANSI_FG_BRIGHT_MAGENTA,
        .accent2 = ANSI_FG_BRIGHT_GREEN
    },
    /* OCEAN - Blue gradient theme */
    {
        .name = "Ocean",
        .prompt_user = ANSI_FG_BRIGHT_CYAN,
        .prompt_host = ANSI_FG_BRIGHT_BLUE,
        .prompt_dir = ANSI_FG_CYAN,
        .prompt_symbol = ANSI_FG_BRIGHT_CYAN,
        .success = ANSI_FG_BRIGHT_GREEN,
        .warning = ANSI_FG_BRIGHT_YELLOW,
        .error = ANSI_FG_BRIGHT_RED,
        .info = ANSI_FG_BRIGHT_BLUE,
        .accent1 = ANSI_FG_BRIGHT_CYAN,
        .accent2 = ANSI_FG_BLUE
    },
    /* FOREST - Green/brown earth tones */
    {
        .name = "Forest",
        .prompt_user = ANSI_FG_BRIGHT_GREEN,
        .prompt_host = ANSI_FG_GREEN,
        .prompt_dir = ANSI_FG_BRIGHT_YELLOW,
        .prompt_symbol = ANSI_FG_BRIGHT_GREEN,
        .success = ANSI_FG_BRIGHT_GREEN,
        .warning = ANSI_FG_BRIGHT_YELLOW,
        .error = ANSI_FG_BRIGHT_RED,
        .info = ANSI_FG_BRIGHT_CYAN,
        .accent1 = ANSI_FG_GREEN,
        .accent2 = ANSI_FG_YELLOW
    },
    /* ASTRA_LIGHT - Clean white with blue accents */
    {
        .name = "Astra Light",
        .prompt_user = ANSI_FG_BLUE,
        .prompt_host = ANSI_FG_MAGENTA,
        .prompt_dir = ANSI_FG_CYAN,
        .prompt_symbol = ANSI_FG_GREEN,
        .success = ANSI_FG_GREEN,
        .warning = ANSI_FG_YELLOW,
        .error = ANSI_FG_RED,
        .info = ANSI_FG_BLUE,
        .accent1 = ANSI_FG_MAGENTA,
        .accent2 = ANSI_FG_CYAN
    }
};

static ThemeID current_theme = THEME_ASTRA_DARK;

void theme_init(void) {
    current_theme = THEME_ASTRA_DARK;
}

void theme_set(ThemeID id) {
    if (id < THEME_COUNT) {
        current_theme = id;
    }
}

ThemeID theme_get_current(void) {
    return current_theme;
}

const ColorTheme* theme_get_active(void) {
    return &themes[current_theme];
}

const char* theme_get_name(ThemeID id) {
    if (id < THEME_COUNT) {
        return themes[id].name;
    }
    return "Unknown";
}
