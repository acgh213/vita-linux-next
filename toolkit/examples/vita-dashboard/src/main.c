/*
 * vita-dashboard — the first interactive framebuffer application on Vita Linux.
 *
 * Lifecycle (contract: docs/plans/vita-dashboard-contract.md):
 *   parse options -> select input by identity -> probe framebuffer geometry ->
 *   allocate one RAM shadow -> save fbcon state -> unbind only if bound ->
 *   render -> write one complete frame -> poll input -> restore ownership on
 *   EVERY exit path, including signals.
 *
 * Deliberately does NOT use the IFTU page-flip path: this first version writes
 * /dev/fb0 directly so the lifecycle can be proven before adding register work.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fb_owner.h"
#include "input.h"
#include "render.h"
#include "status.h"

#define DEFAULT_DURATION_MS 60000u
#define MAX_DURATION_MS     600000u
#define POLL_SLICE_MS       120
#define SCREEN_COUNT        4

/* Signal-safe stop flag: handlers only set this. */
static volatile sig_atomic_t stop_requested;

static void handle_signal(int signum)
{
    (void)signum;
    stop_requested = 1;
}

static unsigned long now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (unsigned long)ts.tv_sec * 1000ul
        + (unsigned long)(ts.tv_nsec / 1000000l);
}

static int streq(const char *a, const char *b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0' && a[i] == b[i])
        i++;
    return a[i] == '\0' && b[i] == '\0';
}

static void usage(void)
{
    printf("usage: vita-dashboard [--machine] [--input-phys ID]\n"
           "                      [--duration-ms N | --forever]\n"
           "                      [--root PREFIX] [--frames-max N]\n");
}

/* ------------------------------------------------------------------ screens */

#define COL_BG      0x101018FFu
#define COL_HEADER  0x30E0C0FFu
#define COL_LABEL   0x8090A0FFu
#define COL_VALUE   0xF0F0F0FFu
#define COL_ACCENT  0xE05080FFu

static const char *screen_title(int screen)
{
    switch (screen) {
    case 0: return "OVERVIEW";
    case 1: return "NETWORK / STORAGE";
    case 2: return "BUILD";
    default: return "INPUT";
    }
}

static void draw_screen(struct renderer *r, int screen,
                        const struct dashboard_status *st,
                        const struct dashboard_input *in,
                        unsigned long runtime_ms,
                        unsigned long frames)
{
    char buffer[128];
    int scale = (r->width >= 1024u) ? 3 : 2;
    int x = 40;
    int y = 40;
    int step = (8 + 1) * scale + 10;

    render_clear(r, COL_BG);

    render_text(r, x, y, "VITA LINUX WORKBENCH", (unsigned int)scale,
                COL_HEADER);
    y += step;
    render_text(r, x, y, screen_title(screen), (unsigned int)scale, COL_ACCENT);
    y += step + 10;

    /* A thin rule under the header. */
    render_rect(r, x, y - 8, (int)r->width - (2 * x), 2, COL_LABEL);
    y += 8;

    switch (screen) {
    case 0:
        render_row(r, x, y, "kernel", st->kernel, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "arch", st->arch, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "cpus online", st->cpus_online,
                   (unsigned int)scale, COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "memory free", st->memory_available,
                   (unsigned int)scale, COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "uptime", st->uptime, (unsigned int)scale,
                   COL_LABEL, COL_VALUE);
        break;
    case 1:
        render_row(r, x, y, "interface", st->interface, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "address", st->address, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "storage", st->storage, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "toolkit", st->toolkit, (unsigned int)scale,
                   COL_LABEL, COL_VALUE);
        break;
    case 2:
        render_row(r, x, y, "compiler", st->compiler, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "workspace", st->workspace, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        snprintf(buffer, sizeof(buffer), "%ux%u stride %u",
                 r->width, r->height, r->stride);
        render_row(r, x, y, "framebuffer", buffer, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        snprintf(buffer, sizeof(buffer), "%lu", frames);
        render_row(r, x, y, "frames written", buffer, (unsigned int)scale,
                   COL_LABEL, COL_VALUE);
        break;
    default:
        render_row(r, x, y, "input device", in->path, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "identity", in->phys, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        render_row(r, x, y, "name", in->name, (unsigned int)scale,
                   COL_LABEL, COL_VALUE); y += step;
        snprintf(buffer, sizeof(buffer), "%lu", in->events_seen);
        render_row(r, x, y, "events seen", buffer, (unsigned int)scale,
                   COL_LABEL, COL_VALUE);
        break;
    }

    /* Footer: controls and the runtime bound, always visible. */
    snprintf(buffer, sizeof(buffer), "screen %d/%d   runtime %lus",
             screen + 1, SCREEN_COUNT, runtime_ms / 1000ul);
    render_text(r, x, (int)r->height - (40 + ((8 + 1) * 2)), buffer, 2,
                COL_LABEL);
    render_text(r, x, (int)r->height - 40,
                "CROSS/SPACE next    CIRCLE/ESC exit", 2, COL_ACCENT);
}

/* --------------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    struct fb_owner owner;
    struct renderer renderer;
    struct dashboard_status status;
    struct dashboard_input input;
    struct sigaction sa;

    const char *root = "";
    const char *wanted_phys = "vita_syscon_buttons";
    unsigned long duration_ms = DEFAULT_DURATION_MS;
    unsigned long frames_max = 0;
    int machine = 0;
    int forever = 0;
    int screen = 0;
    int screens_visited = 1;
    int have_input = 0;
    int rc = 0;
    int i;
    unsigned long started;
    unsigned long runtime = 0;
    const char *status_text = "ok";

    for (i = 1; i < argc; i++) {
        if (streq(argv[i], "--machine")) {
            machine = 1;
        } else if (streq(argv[i], "--forever")) {
            forever = 1;
        } else if (streq(argv[i], "--input-phys") && i + 1 < argc) {
            wanted_phys = argv[++i];
        } else if (streq(argv[i], "--root") && i + 1 < argc) {
            root = argv[++i];
        } else if (streq(argv[i], "--duration-ms") && i + 1 < argc) {
            long value = atol(argv[++i]);

            if (value <= 0 || (unsigned long)value > MAX_DURATION_MS) {
                fprintf(stderr,
                        "vita-dashboard: duration must be 1..%u ms\n",
                        MAX_DURATION_MS);
                return 2;
            }
            duration_ms = (unsigned long)value;
        } else if (streq(argv[i], "--frames-max") && i + 1 < argc) {
            long value = atol(argv[++i]);

            if (value <= 0) {
                fprintf(stderr, "vita-dashboard: --frames-max must be > 0\n");
                return 2;
            }
            frames_max = (unsigned long)value;
        } else if (streq(argv[i], "--help") || streq(argv[i], "-h")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "vita-dashboard: unknown option: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    /* Input identity first: refuse to run rather than guess a device.
     * The device root is derived from --root so fixtures can exercise the real
     * selection path against a fake tree. */
    {
        char devroot[256];

        snprintf(devroot, sizeof(devroot), "%s/dev/input", root);
        if (input_open(&input, root, devroot, wanted_phys) == 0) {
            have_input = 1;
        } else {
            fprintf(stderr,
                    "vita-dashboard: no input device with phys=%s\n",
                    wanted_phys);
            return 3;
        }
    }

    fb_owner_init(&owner, root);
    if (fb_owner_probe(&owner) != 0) {
        fprintf(stderr, "vita-dashboard: %s\n", fb_owner_error(&owner));
        input_close(&input);
        return 4;
    }

    if (render_init(&renderer, owner.width, owner.height, owner.bpp,
                    owner.stride) != 0) {
        fprintf(stderr, "vita-dashboard: cannot allocate the shadow buffer\n");
        input_close(&input);
        return 5;
    }

    /* Install handlers BEFORE taking ownership so no path can leak it. */
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (fb_owner_acquire(&owner) != 0) {
        fprintf(stderr, "vita-dashboard: %s\n", fb_owner_error(&owner));
        render_free(&renderer);
        input_close(&input);
        return 6;
    }

    status_collect(&status, root);
    started = now_ms();

    for (;;) {
        enum dashboard_action action = DASHBOARD_ACTION_NONE;

        runtime = now_ms() - started;

        if (stop_requested) {
            status_text = "signal";
            break;
        }
        if (!forever && runtime >= duration_ms) {
            status_text = "duration_reached";
            break;
        }
        if (frames_max != 0
                && (unsigned long)owner.frames_written >= frames_max) {
            status_text = "frames_reached";
            break;
        }

        draw_screen(&renderer, screen, &status, &input, runtime,
                    (unsigned long)owner.frames_written);

        if (fb_owner_write_frame(&owner, renderer.shadow,
                                 renderer.frame_bytes) != 0) {
            fprintf(stderr, "vita-dashboard: %s\n", fb_owner_error(&owner));
            status_text = "frame_write_failed";
            rc = 7;
            break;
        }

        if (have_input && input_poll(&input, POLL_SLICE_MS, &action) != 0) {
            status_text = "input_error";
            rc = 8;
            break;
        }

        if (action == DASHBOARD_ACTION_EXIT) {
            status_text = "user_exit";
            break;
        }
        if (action == DASHBOARD_ACTION_NEXT) {
            screen = (screen + 1) % SCREEN_COUNT;
            screens_visited++;
            /* Refresh facts when the user asks for a new screen. */
            status_collect(&status, root);
        }
    }

    runtime = now_ms() - started;

    /* Restore ownership on EVERY path, including the error paths above. */
    if (fb_owner_release(&owner) != 0) {
        fprintf(stderr, "vita-dashboard: %s\n", fb_owner_error(&owner));
        if (rc == 0)
            rc = 9;
    }

    render_free(&renderer);
    input_close(&input);

    if (machine) {
        printf("schema=1\n");
        printf("application=vita-dashboard\n");
        printf("framebuffer=%ux%ux%u\n", owner.width, owner.height, owner.bpp);
        printf("stride=%u\n", owner.stride);
        printf("frame_bytes=%lu\n", (unsigned long)owner.frame_bytes);
        printf("input_identity=%s\n", input.phys);
        printf("input_device=%s\n", input.path);
        printf("input_name=%s\n", input.name);
        printf("fbcon_was_bound=%d\n", owner.fbcon_was_bound);
        printf("fbcon_restored=%d\n", owner.fbcon_restored);
        printf("frames_written=%d\n", owner.frames_written);
        printf("screens_visited=%d\n", screens_visited);
        printf("input_events=%lu\n", input.events_seen);
        printf("clipped_writes=%lu\n", renderer.clipped_writes);
        printf("runtime_ms=%lu\n", runtime);
        printf("runtime_bounded=%s\n",
               (forever || runtime <= duration_ms + 5000ul) ? "PASS" : "FAIL");
        printf("status=%s\n", status_text);
    } else {
        printf("vita-dashboard: %s after %lu ms, %d frames, fbcon_restored=%d\n",
               status_text, runtime, owner.frames_written, owner.fbcon_restored);
    }

    if (fflush(stdout) != 0)
        return 10;
    return rc;
}
