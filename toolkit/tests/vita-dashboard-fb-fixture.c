/*
 * Host fixture for the framebuffer ownership state machine.
 * Compiles the ACTUAL production fb_owner.c against fake sysfs trees.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../toolkit/examples/vita-dashboard/src/fb_owner.h"

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

static int read_bind(const char *root)
{
    char path[512];
    FILE *fp;
    int value = -1;

    snprintf(path, sizeof(path), "%s/sys/class/vtconsole/vtcon1/bind", root);
    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%d", &value) != 1)
        value = -1;
    fclose(fp);
    return value;
}

static unsigned char *make_frame(size_t len, unsigned char fill)
{
    unsigned char *buf = malloc(len);
    size_t i;

    if (buf == NULL)
        return NULL;
    for (i = 0; i < len; i++)
        buf[i] = fill;
    return buf;
}

int main(int argc, char **argv)
{
    const char *root;
    const char *mode;
    struct fb_owner owner;
    unsigned char *frame;

    if (argc < 3) {
        fprintf(stderr, "usage: %s ROOT MODE\n", argv[0]);
        return 2;
    }
    root = argv[1];
    mode = argv[2];

    fb_owner_init(&owner, root);

    /* ---- refuse-before-write cases -------------------------------------- */

    if (argv[2][0] == 'b' && argv[2][1] == 'a') { /* "bad-geometry" */
        check(fb_owner_probe(&owner) != 0, "malformed geometry is refused");
        check(owner.state == FB_OWNER_FAILED, "state is FAILED after a bad probe");
        check(read_bind(root) == 1, "fbcon was NOT unbound on a failed probe");
        printf("error=%s\n", fb_owner_error(&owner));
        return failures ? 1 : 0;
    }

    if (argv[2][0] == 's') { /* "short-fb" */
        check(fb_owner_probe(&owner) != 0, "short framebuffer is refused");
        check(owner.frames_written == 0, "no frame was written");
        check(read_bind(root) == 1, "fbcon was NOT unbound for a short framebuffer");
        printf("error=%s\n", fb_owner_error(&owner));
        return failures ? 1 : 0;
    }

    /* ---- normal lifecycles ---------------------------------------------- */

    check(fb_owner_probe(&owner) == 0, "probe succeeds");
    if (owner.state == FB_OWNER_FAILED) {
        printf("error=%s\n", fb_owner_error(&owner));
        return 1;
    }
    printf("geometry=%ux%ux%u stride=%u frame_bytes=%lu\n",
           owner.width, owner.height, owner.bpp, owner.stride,
           (unsigned long)owner.frame_bytes);

    /* A wrong-size frame must be refused before ownership is even taken. */
    check(fb_owner_write_frame(&owner, (const unsigned char *)"x", 1) != 0,
          "write without ownership is refused");

    check(fb_owner_acquire(&owner) == 0, "acquire succeeds");

    if (mode[0] == 'b') { /* "bound" */
        check(owner.fbcon_was_bound == 1, "probe saw fbcon bound");
        check(owner.fbcon_unbound_by_us == 1, "bound console was unbound by us");
        check(read_bind(root) == 0, "fbcon is now unbound");
    } else { /* "unbound" */
        check(owner.fbcon_was_bound == 0, "probe saw fbcon already unbound");
        check(owner.fbcon_unbound_by_us == 0, "an unbound console was not touched");
        check(read_bind(root) == 0, "fbcon remains unbound");
    }

    /* Short and over-long frames are both refused, with no partial write. */
    frame = make_frame(owner.frame_bytes, 0xAB);
    if (frame == NULL) {
        fprintf(stderr, "allocation failed\n");
        return 2;
    }
    check(fb_owner_write_frame(&owner, frame, owner.frame_bytes - 1) != 0,
          "a short frame is refused");
    check(owner.frames_written == 0, "no partial frame was written");

    /* A refused write must NOT have poisoned the owner: no state reset here. */
    check(owner.state == FB_OWNER_OWNED, "a refused write leaves the owner usable");
    check(fb_owner_write_frame(&owner, frame, owner.frame_bytes) == 0,
          "a complete frame is written");
    check(owner.frames_written == 1, "frame counter advanced");

    check(fb_owner_release(&owner) == 0, "release succeeds");
    check(owner.fbcon_restored == 1, "ownership was restored");

    if (mode[0] == 'b')
        check(read_bind(root) == 1, "originally-bound console was rebound");
    else
        check(read_bind(root) == 0, "originally-unbound console stayed unbound");

    /* Release is idempotent. */
    check(fb_owner_release(&owner) == 0, "release is idempotent");

    free(frame);
    return failures ? 1 : 0;
}
