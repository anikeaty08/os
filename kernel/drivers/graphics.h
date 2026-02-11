#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

/* Screen dimension helpers */
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_center_x(const char *text);
uint32_t fb_center_y(uint32_t content_height);
void fb_put_at(uint32_t x, uint32_t y, const char *text);
void fb_clear(void);

#endif /* GRAPHICS_H */
