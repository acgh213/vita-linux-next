/*
 * fb_owner.c — framebuffer ownership state machine.
 *
 * Deliberately avoids <string.h> helpers (memset/strcspn/strncpy): glibc's
 * ifunc-dispatched string routines are not reliably usable from the on-device
 * TinyCC on this target — strcspn() fails to relocate at link time and memset()
 * has been observed to segfault at the call site.  See
 * toolkit/examples/fb-safe/README.md for the isolation evidence.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fb_owner.h"

#define FB_MAX_FRAME_BYTES (64u * 1024u * 1024u)

static void sset(char *dst, size_t cap, const char *src)
{
    size_t i = 0;

    if (cap == 0)
        return;
    while (src != NULL && src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void sappend(char *dst, size_t cap, const char *src)
{
    size_t i = 0;

    while (i < cap && dst[i] != '\0')
        i++;
    while (src != NULL && *src != '\0' && i + 1 < cap) {
        dst[i] = *src;
        i++;
        src++;
    }
    if (i < cap)
        dst[i] = '\0';
}

/* A hard fault: the owner is no longer usable. */
static void set_error(struct fb_owner *owner, const char *message)
{
    sset(owner->error, sizeof(owner->error), message);
    owner->state = FB_OWNER_FAILED;
}

/*
 * A refusal: the caller asked for something invalid, but nothing was touched
 * and the owner remains in its current state.  Argument validation must NOT
 * poison an otherwise healthy owner.
 */
static void refuse(struct fb_owner *owner, const char *message)
{
    sset(owner->error, sizeof(owner->error), message);
}

static void join(char *dst, size_t cap, const char *root, const char *tail)
{
    sset(dst, cap, root != NULL ? root : "");
    sappend(dst, cap, tail);
}

static int read_uint_file(const char *path, unsigned int *out)
{
    FILE *fp = fopen(path, "r");
    unsigned int value = 0;

    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%u", &value) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *out = value;
    return 0;
}

static int read_size_file(const char *path, unsigned int *w, unsigned int *h)
{
    FILE *fp = fopen(path, "r");
    unsigned int a = 0, b = 0;

    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%u,%u", &a, &b) != 2) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *w = a;
    *h = b;
    return 0;
}

void fb_owner_init(struct fb_owner *owner, const char *root)
{
    owner->width = 0;
    owner->height = 0;
    owner->bpp = 0;
    owner->stride = 0;
    owner->frame_bytes = 0;
    owner->fbcon_was_bound = -1;
    owner->fbcon_unbound_by_us = 0;
    owner->fbcon_restored = 0;
    owner->frames_written = 0;
    owner->state = FB_OWNER_IDLE;
    owner->error[0] = '\0';

    join(owner->fb_path, sizeof(owner->fb_path), root, "/dev/fb0");
    join(owner->vtcon_bind_path, sizeof(owner->vtcon_bind_path), root,
         "/sys/class/vtconsole/vtcon1/bind");
    join(owner->sysfs_size_path, sizeof(owner->sysfs_size_path), root,
         "/sys/class/graphics/fb0/virtual_size");
    join(owner->sysfs_bpp_path, sizeof(owner->sysfs_bpp_path), root,
         "/sys/class/graphics/fb0/bits_per_pixel");
    join(owner->sysfs_stride_path, sizeof(owner->sysfs_stride_path), root,
         "/sys/class/graphics/fb0/stride");
}

int fb_owner_probe(struct fb_owner *owner)
{
    unsigned int w = 0, h = 0, bpp = 0, stride = 0;
    FILE *fp;

    if (owner->state != FB_OWNER_IDLE) {
        set_error(owner, "probe called from a non-idle state");
        return -1;
    }

    if (read_size_file(owner->sysfs_size_path, &w, &h) != 0) {
        set_error(owner, "cannot read framebuffer virtual_size");
        return -1;
    }
    if (read_uint_file(owner->sysfs_bpp_path, &bpp) != 0) {
        set_error(owner, "cannot read framebuffer bits_per_pixel");
        return -1;
    }
    if (w == 0 || h == 0 || bpp == 0 || (bpp % 8u) != 0) {
        set_error(owner, "unsupported framebuffer geometry");
        return -1;
    }
    if (read_uint_file(owner->sysfs_stride_path, &stride) != 0 || stride == 0)
        stride = w * (bpp / 8u);
    if (stride < w * (bpp / 8u)) {
        set_error(owner, "stride smaller than one packed row");
        return -1;
    }

    owner->width = w;
    owner->height = h;
    owner->bpp = bpp;
    owner->stride = stride;
    owner->frame_bytes = (size_t)stride * h;

    if (owner->frame_bytes == 0 || owner->frame_bytes > FB_MAX_FRAME_BYTES) {
        set_error(owner, "implausible frame size");
        return -1;
    }

    /* The framebuffer must exist and hold at least one full frame BEFORE we
     * consider unbinding the console.
     *
     * NOTE: fseek/ftell is NOT a valid size probe here.  On the real device
     * /dev/fb0 is a character device and ftell() returns 0 even though a full
     * 3,686,400-byte frame is readable (measured on PSTV).  Size must come
     * from fstat() for regular files (host fixtures) and from
     * FBIOGET_FSCREENINFO for the actual device. */
    {
        struct stat st;
        int fd = open(owner->fb_path, O_RDONLY);

        if (fd < 0) {
            set_error(owner, "cannot open framebuffer device");
            return -1;
        }
        if (fstat(fd, &st) != 0) {
            close(fd);
            set_error(owner, "cannot stat framebuffer device");
            return -1;
        }
        if (S_ISREG(st.st_mode)) {
            if ((size_t)st.st_size < owner->frame_bytes) {
                close(fd);
                set_error(owner, "framebuffer is shorter than one frame");
                return -1;
            }
        } else if (S_ISCHR(st.st_mode)) {
            struct fb_fix_screeninfo fix;

            /* If the driver answers, trust smem_len; if it does not, the
             * sysfs geometry already validated above is authoritative. */
            if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0
                    && fix.smem_len != 0
                    && (size_t)fix.smem_len < owner->frame_bytes) {
                close(fd);
                set_error(owner, "framebuffer is shorter than one frame");
                return -1;
            }
        } else {
            close(fd);
            set_error(owner, "framebuffer path is not a device or regular file");
            return -1;
        }
        close(fd);
    }

    /* fbcon bind state: absent node means there is no console to displace. */
    owner->fbcon_was_bound = 0;
    fp = fopen(owner->vtcon_bind_path, "r");
    if (fp != NULL) {
        int value = 0;

        if (fscanf(fp, "%d", &value) == 1)
            owner->fbcon_was_bound = (value != 0);
        fclose(fp);
    }

    owner->state = FB_OWNER_PROBED;
    return 0;
}

static int write_bind(struct fb_owner *owner, int value)
{
    FILE *fp = fopen(owner->vtcon_bind_path, "w");

    if (fp == NULL)
        return -1;
    if (fprintf(fp, "%d", value) < 0) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0)
        return -1;
    return 0;
}

int fb_owner_acquire(struct fb_owner *owner)
{
    if (owner->state != FB_OWNER_PROBED) {
        set_error(owner, "acquire called before a successful probe");
        return -1;
    }

    /* Unbind ONLY if it was bound; an already-unbound console stays unbound. */
    if (owner->fbcon_was_bound == 1) {
        if (write_bind(owner, 0) != 0) {
            set_error(owner, "cannot unbind fbcon");
            return -1;
        }
        owner->fbcon_unbound_by_us = 1;
    }

    owner->state = FB_OWNER_OWNED;
    return 0;
}

int fb_owner_write_frame(struct fb_owner *owner, const unsigned char *frame,
                         size_t len)
{
    int fd;
    size_t written;

    if (owner->state != FB_OWNER_OWNED) {
        refuse(owner, "write_frame called without ownership");
        return -1;
    }
    if (frame == NULL) {
        refuse(owner, "null frame");
        return -1;
    }
    /* A partial frame is never acceptable. */
    if (len != owner->frame_bytes) {
        refuse(owner, "frame length does not match stride*height");
        return -1;
    }

    /* POSIX I/O with an explicit short-write loop: write(2) on a framebuffer
     * device may legitimately return fewer bytes than requested, and a partial
     * frame must never be left on screen. */
    fd = open(owner->fb_path, O_WRONLY);
    if (fd < 0) {
        set_error(owner, "cannot open framebuffer for writing");
        return -1;
    }
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        close(fd);
        set_error(owner, "cannot seek framebuffer");
        return -1;
    }

    written = 0;
    while (written < len) {
        ssize_t chunk = write(fd, frame + written, len - written);

        if (chunk < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            set_error(owner, "framebuffer write failed");
            return -1;
        }
        if (chunk == 0)
            break;
        written += (size_t)chunk;
    }
    if (written != len) {
        close(fd);
        set_error(owner, "short framebuffer write");
        return -1;
    }
    close(fd);

    owner->frames_written++;
    return 0;
}

int fb_owner_release(struct fb_owner *owner)
{
    int rc = 0;

    if (owner->state == FB_OWNER_RELEASED)
        return 0;

    /* Restore exactly what we changed, and only what we changed. */
    if (owner->fbcon_unbound_by_us) {
        if (write_bind(owner, 1) != 0) {
            sset(owner->error, sizeof(owner->error), "cannot rebind fbcon");
            rc = -1;
        } else {
            owner->fbcon_unbound_by_us = 0;
            owner->fbcon_restored = 1;
        }
    } else if (owner->fbcon_was_bound == 0) {
        /* Nothing was displaced; the original state is already in force. */
        owner->fbcon_restored = 1;
    }

    if (rc == 0)
        owner->state = FB_OWNER_RELEASED;
    return rc;
}

const char *fb_owner_error(const struct fb_owner *owner)
{
    return owner->error;
}
