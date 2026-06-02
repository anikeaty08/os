#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

/* Screen dimension helpers */
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pixel_width(void);
uint32_t fb_get_pixel_height(void);
uint32_t fb_center_x(const char *text);
uint32_t fb_center_y(uint32_t content_height);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_text_px(uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg);
void fb_put_at(uint32_t x, uint32_t y, const char *text);
void fb_clear(void);

#endif /* GRAPHICS_H */
