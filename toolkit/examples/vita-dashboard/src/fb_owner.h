#ifndef VITA_DASHBOARD_FB_OWNER_H
#define VITA_DASHBOARD_FB_OWNER_H

#include <stddef.h>

/*
 * Framebuffer ownership state machine.
 *
 * Ownership rules (verified on PSTV, see references/demo-cart-framebuffer):
 *   - read geometry before touching anything;
 *   - unbind fbcon ONLY when it was originally bound;
 *   - render into RAM, never read the framebuffer while rendering;
 *   - write exactly stride*height bytes, or refuse;
 *   - restore the original bind state on every exit path.
 *
 * Every path is parameterised so host fixtures can point it at a fake sysfs
 * tree and a regular file standing in for /dev/fb0.
 */

#define FB_OWNER_ERR_LEN 160

enum fb_owner_state {
    FB_OWNER_IDLE = 0,
    FB_OWNER_PROBED,
    FB_OWNER_OWNED,
    FB_OWNER_RELEASED,
    FB_OWNER_FAILED
};

struct fb_owner {
    char fb_path[256];
    char vtcon_bind_path[256];
    char sysfs_size_path[256];
    char sysfs_bpp_path[256];
    char sysfs_stride_path[256];

    unsigned int width;
    unsigned int height;
    unsigned int bpp;
    unsigned int stride;
    size_t frame_bytes;

    int fbcon_was_bound;   /* -1 unknown, 0 unbound, 1 bound */
    int fbcon_unbound_by_us;
    int fbcon_restored;
    int frames_written;

    enum fb_owner_state state;
    char error[FB_OWNER_ERR_LEN];
};

/* Point the owner at a root prefix ("" for the real system). */
void fb_owner_init(struct fb_owner *owner, const char *root);

/* Read geometry and the current fbcon bind state. IDLE -> PROBED. */
int fb_owner_probe(struct fb_owner *owner);

/* Take ownership: unbind fbcon only if it was bound. PROBED -> OWNED. */
int fb_owner_acquire(struct fb_owner *owner);

/*
 * Write one complete frame. Refuses unless len == frame_bytes and the
 * framebuffer is at least that large. Never performs a partial write.
 */
int fb_owner_write_frame(struct fb_owner *owner, const unsigned char *frame,
                         size_t len);

/* Restore the original bind state. Idempotent. -> RELEASED. */
int fb_owner_release(struct fb_owner *owner);

const char *fb_owner_error(const struct fb_owner *owner);

#endif /* VITA_DASHBOARD_FB_OWNER_H */
