/*
 * fb-safe — the safe framebuffer render path, without touching /dev/fb0.
 *
 * This example deliberately performs NO framebuffer write.  It reads geometry
 * metadata from sysfs, allocates a RAM shadow buffer, renders a test pattern
 * into that buffer, and reports the byte count it WOULD have written.
 *
 * The rule it teaches: never read the framebuffer as part of rendering (reads
 * run ~6x slower than writes on this hardware), and never write a partial
 * frame.  Ownership of /dev/fb0 belongs to the dashboard, which unbinds fbcon
 * first; see toolkit/examples/vita-dashboard.
 */
#include <stdio.h>
#include <stdlib.h>

#define FB_SYSFS_SIZE "/sys/class/graphics/fb0/virtual_size"
#define FB_SYSFS_BPP  "/sys/class/graphics/fb0/bits_per_pixel"
#define FB_SYSFS_STRIDE "/sys/class/graphics/fb0/stride"

struct fb_geometry {
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
    unsigned int stride;
};

static int read_line(const char *path, char *out, size_t len)
{
    FILE *fp = fopen(path, "r");
    size_t i;

    if (fp == NULL)
        return -1;
    if (fgets(out, (int)len, fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Trim the trailing newline manually.  strcspn() is an ifunc-dispatched
     * glibc symbol that TinyCC cannot relocate on this target
     * ("can't relocate value at ..."), so the on-device compiler would fail to
     * link.  A plain loop keeps the example buildable by both toolchains. */
    for (i = 0; i < len && out[i] != '\0'; i++) {
        if (out[i] == '\r' || out[i] == '\n') {
            out[i] = '\0';
            break;
        }
    }
    return 0;
}

static int read_geometry(struct fb_geometry *geo)
{
    char buffer[64];
    unsigned int w = 0, h = 0, bpp = 0;

    /* Field-wise zeroing rather than memset().  On this target, a memset()
     * call in THIS translation unit reproducibly segfaults when built with the
     * on-device TinyCC (independent of -lm/-lpthread), while the identical
     * source is clean under host arm-linux-gnueabihf-gcc.  Together with
     * strcspn()'s "can't relocate value" link error, treat glibc's
     * ifunc-dispatched string routines as unavailable to tcc here and write
     * the loop out explicitly.  See toolkit/examples/fb-safe/README.md. */
    geo->width = 0;
    geo->height = 0;
    geo->bpp = 0;
    geo->stride = 0;

    if (read_line(FB_SYSFS_SIZE, buffer, sizeof(buffer)) != 0)
        return -1;
    if (sscanf(buffer, "%u,%u", &w, &h) != 2)
        return -1;

    if (read_line(FB_SYSFS_BPP, buffer, sizeof(buffer)) != 0)
        return -1;
    if (sscanf(buffer, "%u", &bpp) != 1)
        return -1;

    if (w == 0 || h == 0 || bpp == 0)
        return -1;

    geo->width = w;
    geo->height = h;
    geo->bpp = bpp;

    /* Prefer the kernel-reported stride; fall back to the packed width. */
    if (read_line(FB_SYSFS_STRIDE, buffer, sizeof(buffer)) == 0
            && sscanf(buffer, "%u", &geo->stride) == 1 && geo->stride != 0)
        return 0;

    geo->stride = w * (bpp / 8u);
    return 0;
}

/* Bounded fill: every write is clipped to the shadow buffer's real extent. */
static void fill_rect(unsigned char *shadow, const struct fb_geometry *geo,
                      unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h, unsigned int rgba)
{
    unsigned int row, col;
    unsigned int bytes = geo->bpp / 8u;

    if (x >= geo->width || y >= geo->height)
        return;
    if (w > geo->width - x)
        w = geo->width - x;
    if (h > geo->height - y)
        h = geo->height - y;

    for (row = y; row < y + h; row++) {
        unsigned char *line = shadow + ((size_t)row * geo->stride);

        for (col = x; col < x + w; col++) {
            unsigned char *pixel = line + ((size_t)col * bytes);

            /* PSTV memory order is RGBA (a8b8g8r8 in DRM naming). */
            pixel[0] = (unsigned char)((rgba >> 24) & 0xFFu);
            pixel[1] = (unsigned char)((rgba >> 16) & 0xFFu);
            pixel[2] = (unsigned char)((rgba >> 8) & 0xFFu);
            if (bytes >= 4)
                pixel[3] = (unsigned char)(rgba & 0xFFu);
        }
    }
}

int main(void)
{
    struct fb_geometry geo;
    unsigned char *shadow;
    size_t frame_bytes;
    unsigned int band;

    if (read_geometry(&geo) != 0) {
        fprintf(stderr, "fb-safe: cannot read framebuffer geometry\n");
        return 2;
    }

    frame_bytes = (size_t)geo.stride * geo.height;
    if (frame_bytes == 0 || frame_bytes > (64u * 1024u * 1024u)) {
        fprintf(stderr, "fb-safe: implausible frame size %lu\n",
                (unsigned long)frame_bytes);
        return 3;
    }

    shadow = calloc(1, frame_bytes);
    if (shadow == NULL) {
        fprintf(stderr, "fb-safe: cannot allocate %lu bytes\n",
                (unsigned long)frame_bytes);
        return 4;
    }

    /* Six horizontal bands, deliberately over-wide so clipping is exercised. */
    for (band = 0; band < 6; band++) {
        static const unsigned int colors[6] = {
            0xFF0000FFu, 0xFF7F00FFu, 0xFFFF00FFu,
            0x00FF00FFu, 0x0000FFFFu, 0x7F00FFFFu
        };
        unsigned int band_h = geo.height / 6u;

        fill_rect(shadow, &geo, 0, band * band_h,
                  geo.width + 64u, band_h, colors[band]);
    }

    printf("schema=1\n");
    printf("example=fb-safe\n");
    printf("framebuffer=%ux%ux%u\n", geo.width, geo.height, geo.bpp);
    printf("stride=%u\n", geo.stride);
    printf("shadow_bytes=%lu\n", (unsigned long)frame_bytes);
    printf("framebuffer_writes=0\n");
    printf("status=ok\n");

    free(shadow);

    if (fflush(stdout) != 0) {
        fprintf(stderr, "fb-safe: stdout flush failed\n");
        return 5;
    }

    return 0;
}
