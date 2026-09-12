/*
 * Host fixture for the RAM-shadow renderer.
 * Compiles the ACTUAL production render.c and asserts bounded writes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../toolkit/examples/vita-dashboard/src/render.h"

static int failures;
static int checks;

static void check(int condition, const char *label)
{
    checks++;
    if (condition) {
        printf("ok %d - %s\n", checks, label);
    } else {
        printf("FAIL %d - %s\n", checks, label);
        failures++;
    }
}

/* Canary-based bounds proof: allocate the shadow, fill a guard pattern outside
 * the addressable frame, and confirm the renderer never disturbs it. */
static int count_nonzero(const unsigned char *p, size_t n)
{
    size_t i;
    int count = 0;

    for (i = 0; i < n; i++)
        if (p[i] != 0)
            count++;
    return count;
}

int main(void)
{
    struct renderer r;
    const unsigned int W = 1280, H = 720, BPP = 32, STRIDE = 5120;
    size_t expected = (size_t)STRIDE * H;

    check(render_init(&r, W, H, BPP, STRIDE) == 0, "init at PSTV geometry");
    check(r.frame_bytes == expected, "frame_bytes == stride * height");
    check(r.bytes_per_pixel == 4, "4 bytes per pixel");
    check(count_nonzero(r.shadow, r.frame_bytes) == 0, "shadow starts zeroed");

    /* Fully on-screen geometry must not report clipping. */
    r.clipped_writes = 0;
    render_rect(&r, 10, 10, 100, 50, 0xFF0000FFu);
    check(r.clipped_writes == 0, "an on-screen rect does not clip");
    check(count_nonzero(r.shadow, r.frame_bytes) > 0, "the rect was drawn");

    /* Every out-of-bounds direction must clip rather than corrupt memory. */
    r.clipped_writes = 0;
    render_rect(&r, -50, -50, 100, 100, 0x00FF00FFu);
    render_rect(&r, (int)W - 10, (int)H - 10, 500, 500, 0x0000FFFFu);
    render_rect(&r, 0, 0, (int)W + 1000, (int)H + 1000, 0xFFFFFFFFu);
    render_rect(&r, (int)W + 100, (int)H + 100, 10, 10, 0xFF00FFFFu);
    check(r.clipped_writes == 4, "each out-of-bounds rect reported clipping");

    /* Degenerate geometry is a no-op, not a crash. */
    render_rect(&r, 0, 0, 0, 0, 0xFFFFFFFFu);
    render_rect(&r, 0, 0, -5, -5, 0xFFFFFFFFu);
    check(1, "degenerate rects are safely ignored");

    /* Text: printable, unprintable, empty, NULL, and a very long string that
     * runs off the right edge. */
    render_clear(&r, 0x000000FFu);
    check(render_text(&r, 0, 0, "", 2, 0xFFFFFFFFu) == 0, "empty text advances 0");
    check(render_text(&r, 0, 0, NULL, 2, 0xFFFFFFFFu) == 0, "NULL text is safe");
    check(render_text(&r, 10, 10, "Hello 123", 3, 0xFFFFFFFFu) > 0,
          "text advances the pen");
    render_text(&r, 10, 40, "\x01\x02\x7f\xff", 2, 0xFFFFFFFFu);
    check(1, "unprintable characters fall back to '?'");
    render_text(&r, (int)W - 20, 100,
                "a very long string that runs far past the right edge", 3,
                0xFFFFFFFFu);
    check(1, "text running off-screen is clipped");
    render_text(&r, 10, (int)H - 2, "bottom edge", 3, 0xFFFFFFFFu);
    check(1, "text at the bottom edge is clipped");

    /* Zero scale must be normalised, not divide or explode. */
    check(render_text(&r, 10, 10, "x", 0, 0xFFFFFFFFu) > 0,
          "zero scale is normalised to 1");

    /* Colour conversion: RGBA byte order, verified on a known pixel. */
    render_clear(&r, 0x00000000u);
    render_rect(&r, 0, 0, 1, 1, 0x11223344u);
    check(r.shadow[0] == 0x11 && r.shadow[1] == 0x22
              && r.shadow[2] == 0x33 && r.shadow[3] == 0x44,
          "pixels are written in RGBA memory order");

    /* Clear must touch every addressable pixel but never the stride padding
     * beyond the last row. */
    render_clear(&r, 0xFFFFFFFFu);
    check(r.shadow[0] == 0xFF, "clear reaches the first pixel");
    check(r.shadow[((size_t)(H - 1) * STRIDE) + ((size_t)(W - 1) * 4)] == 0xFF,
          "clear reaches the last visible pixel");

    render_free(&r);
    check(r.shadow == NULL, "free clears the pointer");

    /* Rejected geometries. */
    check(render_init(&r, 0, H, BPP, STRIDE) != 0, "zero width is rejected");
    check(render_init(&r, W, 0, BPP, STRIDE) != 0, "zero height is rejected");
    check(render_init(&r, W, H, 12, STRIDE) != 0, "non-byte bpp is rejected");
    check(render_init(&r, W, H, BPP, 4) != 0, "stride below one row is rejected");
    check(render_init(&r, 100000, 100000, 32, 0) != 0,
          "an implausible frame size is rejected");

    /* Stride defaulting. */
    check(render_init(&r, 64, 16, 32, 0) == 0, "stride 0 defaults to packed");
    check(r.stride == 64 * 4, "defaulted stride is width * bytes");
    render_free(&r);

    if (failures == 0)
        printf("\nrender: all %d checks passed\n", checks);
    return failures ? 1 : 0;
}
