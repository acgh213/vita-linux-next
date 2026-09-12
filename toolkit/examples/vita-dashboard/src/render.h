#ifndef VITA_DASHBOARD_RENDER_H
#define VITA_DASHBOARD_RENDER_H

#include <stddef.h>

/*
 * RAM-shadow renderer.
 *
 * Rendering NEVER reads the framebuffer: measured on PSTV, framebuffer reads
 * run ~25.7 MB/s against ~167 MB/s for writes.  Everything is composed in RAM
 * and handed to fb_owner as one complete frame.
 */

struct renderer {
    unsigned char *shadow;
    size_t frame_bytes;
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    unsigned int bytes_per_pixel;
    unsigned long clipped_writes;  /* diagnostic: how often clipping engaged */
};

int render_init(struct renderer *r, unsigned int width, unsigned int height,
                unsigned int bpp, unsigned int stride);
void render_free(struct renderer *r);

void render_clear(struct renderer *r, unsigned int rgba);
void render_rect(struct renderer *r, int x, int y, int w, int h,
                 unsigned int rgba);

/* 8x8 bitmap font, integer-scaled. Returns the advance in pixels. */
int render_text(struct renderer *r, int x, int y, const char *text,
                unsigned int scale, unsigned int rgba);

/* Convenience: "label  value" on one row. */
void render_row(struct renderer *r, int x, int y, const char *label,
                const char *value, unsigned int scale,
                unsigned int label_rgba, unsigned int value_rgba);

#endif /* VITA_DASHBOARD_RENDER_H */
