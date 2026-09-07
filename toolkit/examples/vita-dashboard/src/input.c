/*
 * input.c — identity-selected evdev input for vita-dashboard.
 *
 * Reuses the identity-first selection rule from vita-inputwatch: the device is
 * chosen by its sysfs `phys` string, never by a hardcoded event number, because
 * event numbering is not stable across boots or USB attachment.
 *
 * The node is opened read-only and non-blocking; a poll timeout is a normal
 * bounded outcome, not an error.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "input.h"

static void sset(char *dst, int cap, const char *src)
{
    int i = 0;

    if (cap <= 0)
        return;
    while (src != NULL && src[i] != '\0' && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int read_text(const char *path, char *out, int cap)
{
    int fd;
    ssize_t length;
    int end;

    if (cap <= 1)
        return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    length = read(fd, out, (size_t)(cap - 1));
    close(fd);
    if (length <= 0)
        return -1;
    out[length] = '\0';
    end = (int)length;
    while (end > 0 && (out[end - 1] == '\n' || out[end - 1] == '\r'))
        out[--end] = '\0';
    return (end > 0) ? 0 : -1;
}

int input_open(struct dashboard_input *in, const char *sysroot,
               const char *devroot, const char *wanted_phys)
{
    unsigned int number;

    in->fd = -1;
    in->number = 0;
    in->name[0] = '\0';
    in->phys[0] = '\0';
    in->path[0] = '\0';
    in->events_seen = 0;
    in->malformed = 0;

    if (wanted_phys == NULL || wanted_phys[0] == '\0')
        return -1;
    if (sysroot == NULL)
        sysroot = "";
    if (devroot == NULL)
        devroot = "/dev/input";

    for (number = 0; number < 128u; number++) {
        char phys_path[INPUT_TEXT];
        char name_path[INPUT_TEXT];
        char node_path[INPUT_TEXT];
        char phys[INPUT_TEXT];
        int fd;

        snprintf(phys_path, sizeof(phys_path),
                 "%s/sys/class/input/event%u/device/phys", sysroot, number);
        if (read_text(phys_path, phys, (int)sizeof(phys)) != 0)
            continue;

        /* Identity match is exact; a partial match is not good enough. */
        {
            int i = 0;

            while (phys[i] != '\0' && wanted_phys[i] != '\0'
                    && phys[i] == wanted_phys[i])
                i++;
            if (phys[i] != '\0' || wanted_phys[i] != '\0')
                continue;
        }

        snprintf(node_path, sizeof(node_path), "%s/event%u", devroot, number);
        fd = open(node_path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        in->fd = fd;
        in->number = number;
        sset(in->phys, INPUT_TEXT, phys);
        sset(in->path, INPUT_TEXT, node_path);

        snprintf(name_path, sizeof(name_path),
                 "%s/sys/class/input/event%u/device/name", sysroot, number);
        if (read_text(name_path, in->name, INPUT_TEXT) != 0)
            sset(in->name, INPUT_TEXT, "UNKNOWN");

        return 0;
    }

    return -1;
}

/* Map a key code to a dashboard action. Unmapped keys are ignored. */
static enum dashboard_action action_for(unsigned short code)
{
    switch (code) {
    /* Exit: CIRCLE on the Vita pad, Esc / q on a keyboard. */
    case BTN_B:
    case KEY_ESC:
    case KEY_Q:
        return DASHBOARD_ACTION_EXIT;
    /* Advance: CROSS / D-pad down, space / enter / tab on a keyboard. */
    case BTN_A:            /* == BTN_SOUTH (CROSS) */
    case KEY_DOWN:
    case KEY_SPACE:
    case KEY_ENTER:
    case KEY_TAB:
        return DASHBOARD_ACTION_NEXT;
    default:
        return DASHBOARD_ACTION_NONE;
    }
}

int input_poll(struct dashboard_input *in, int timeout_ms,
               enum dashboard_action *action)
{
    struct pollfd pfd;
    struct input_event event;
    ssize_t got;
    int ready;

    *action = DASHBOARD_ACTION_NONE;
    if (in->fd < 0)
        return -1;
    if (timeout_ms < 0)
        timeout_ms = 0;

    pfd.fd = in->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    ready = poll(&pfd, 1, timeout_ms);
    if (ready < 0) {
        /* A signal arriving during poll() is a normal interruption, not an
         * input error: the caller's stop flag is already set and it will exit
         * through the ownership-restoring path. */
        if (errno == EINTR)
            return 0;
        return -1;
    }
    if (ready == 0)
        return 0;                 /* timeout: a normal bounded result */
    if ((pfd.revents & POLLIN) == 0)
        return 0;

    /* Drain what is available; the last actionable key wins. */
    for (;;) {
        got = read(in->fd, &event, sizeof(event));
        if (got <= 0)
            break;
        if ((size_t)got != sizeof(event)) {
            in->malformed++;
            break;
        }
        in->events_seen++;

        /* value 1 = press, 2 = autorepeat; releases are ignored. */
        if (event.type == EV_KEY && (event.value == 1 || event.value == 2)) {
            enum dashboard_action mapped = action_for(event.code);

            if (mapped != DASHBOARD_ACTION_NONE)
                *action = mapped;
        }
    }

    return 0;
}

void input_close(struct dashboard_input *in)
{
    if (in->fd >= 0) {
        close(in->fd);
        in->fd = -1;
    }
}
