/*
 * status.c — read-only data providers for vita-dashboard.
 *
 * Reads /proc and /sys directly.  Does not shell out, does not open a socket,
 * does not ping.  Every read is bounded and every field falls back to UNKNOWN,
 * so the dashboard stays functional with no storage, no network, and no
 * toolkit mounted.
 *
 * Avoids <string.h> helpers deliberately (see fb-safe/README.md).
 */
#include <stdio.h>

#include "status.h"

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

static void unknown(char *dst)
{
    sset(dst, STATUS_FIELD, "UNKNOWN");
}

static void path_join(char *dst, int cap, const char *root, const char *tail)
{
    int i = 0, j = 0;

    if (cap <= 0)
        return;
    while (root != NULL && root[i] != '\0' && j < cap - 1)
        dst[j++] = root[i++];
    i = 0;
    while (tail[i] != '\0' && j < cap - 1)
        dst[j++] = tail[i++];
    dst[j] = '\0';
}

/* Read the first line of a file, trimming trailing whitespace. */
static int read_first_line(const char *path, char *out, int cap)
{
    FILE *fp = fopen(path, "r");
    int i;

    if (fp == NULL)
        return -1;
    if (fgets(out, cap, fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    for (i = 0; i < cap && out[i] != '\0'; i++) {
        if (out[i] == '\r' || out[i] == '\n') {
            out[i] = '\0';
            break;
        }
    }
    return (out[0] == '\0') ? -1 : 0;
}

static void read_or_unknown(const char *root, const char *tail, char *out)
{
    char path[256];

    path_join(path, (int)sizeof(path), root, tail);
    if (read_first_line(path, out, STATUS_FIELD) != 0)
        unknown(out);
}

static void collect_uname(struct dashboard_status *status, const char *root)
{
    char path[256];
    FILE *fp;

    path_join(path, (int)sizeof(path), root, "/proc/sys/kernel/osrelease");
    if (read_first_line(path, status->kernel, STATUS_FIELD) != 0)
        unknown(status->kernel);

    /* /proc/cpuinfo "model name" or "Processor" is unreliable across arches;
     * report the machine from the kernel's own arch marker when present. */
    path_join(path, (int)sizeof(path), root, "/proc/sys/kernel/arch");
    if (read_first_line(path, status->arch, STATUS_FIELD) != 0) {
        path_join(path, (int)sizeof(path), root, "/proc/cpuinfo");
        fp = fopen(path, "r");
        unknown(status->arch);
        if (fp != NULL) {
            char line[256];

            while (fgets(line, (int)sizeof(line), fp) != NULL) {
                if (line[0] == 'C' && line[1] == 'P' && line[2] == 'U'
                        && line[3] == ' ' && line[4] == 'a') {
                    /* "CPU architecture: 7" */
                    char *p = line;

                    while (*p != '\0' && *p != ':')
                        p++;
                    if (*p == ':') {
                        p++;
                        while (*p == ' ')
                            p++;
                        sset(status->arch, STATUS_FIELD, p);
                        for (int i = 0; i < STATUS_FIELD; i++) {
                            if (status->arch[i] == '\n'
                                    || status->arch[i] == '\r') {
                                status->arch[i] = '\0';
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            fclose(fp);
        }
    }
}

static void collect_memory(struct dashboard_status *status, const char *root)
{
    char path[256];
    char line[256];
    FILE *fp;

    unknown(status->memory_available);
    path_join(path, (int)sizeof(path), root, "/proc/meminfo");
    fp = fopen(path, "r");
    if (fp == NULL)
        return;
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        unsigned long value = 0;

        if (sscanf(line, "MemAvailable: %lu kB", &value) == 1) {
            snprintf(status->memory_available, STATUS_FIELD, "%lu MiB",
                     value / 1024u);
            break;
        }
    }
    fclose(fp);
}

static void collect_uptime(struct dashboard_status *status, const char *root)
{
    char path[256];
    FILE *fp;
    double seconds = 0.0;

    unknown(status->uptime);
    path_join(path, (int)sizeof(path), root, "/proc/uptime");
    fp = fopen(path, "r");
    if (fp == NULL)
        return;
    if (fscanf(fp, "%lf", &seconds) == 1) {
        unsigned long total = (unsigned long)seconds;

        snprintf(status->uptime, STATUS_FIELD, "%luh %lum %lus",
                 total / 3600u, (total % 3600u) / 60u, total % 60u);
    }
    fclose(fp);
}

/* First non-loopback interface reporting operstate "up". */
static void collect_network(struct dashboard_status *status, const char *root)
{
    static const char *candidates[] = { "mlan0", "eth0", "wlan0", "usb0" };
    char path[256];
    char state[STATUS_FIELD];
    unsigned int i;

    unknown(status->interface);
    unknown(status->address);

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        snprintf(path, sizeof(path), "%s/sys/class/net/%s/operstate",
                 root, candidates[i]);
        if (read_first_line(path, state, STATUS_FIELD) != 0)
            continue;
        if (state[0] != 'u' || state[1] != 'p')
            continue;
        sset(status->interface, STATUS_FIELD, candidates[i]);
        break;
    }

    if (status->interface[0] == 'U')
        return;

    /* /proc/net/fib_trie and friends are awkward to parse; the address is
     * reported by the launcher via the environment when available, so an
     * UNKNOWN here is honest rather than guessed. */
    snprintf(path, sizeof(path), "%s/sys/class/net/%s/address",
             root, status->interface);
    if (read_first_line(path, status->address, STATUS_FIELD) != 0)
        unknown(status->address);
}

static void collect_mounts(struct dashboard_status *status, const char *root)
{
    char path[256];
    char line[512];
    FILE *fp;

    unknown(status->storage);
    unknown(status->toolkit);

    path_join(path, (int)sizeof(path), root, "/proc/mounts");
    fp = fopen(path, "r");
    if (fp == NULL)
        return;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char device[128];
        char mountpoint[128];
        char fstype[64];
        char options[128];

        if (sscanf(line, "%127s %127s %63s %127s",
                   device, mountpoint, fstype, options) != 4)
            continue;

        if (mountpoint[1] == 'm' && mountpoint[2] == 'n'
                && mountpoint[3] == 't')
            snprintf(status->storage, STATUS_FIELD, "%.20s %.60s", fstype, options);

        if (mountpoint[1] == 'o' && mountpoint[2] == 'p'
                && mountpoint[3] == 't')
            snprintf(status->toolkit, STATUS_FIELD, "%.20s %.60s", fstype, options);
    }
    fclose(fp);
}

void status_collect(struct dashboard_status *status, const char *root)
{
    char path[256];

    if (root == NULL)
        root = "";

    collect_uname(status, root);
    read_or_unknown(root, "/sys/devices/system/cpu/online", status->cpus_online);
    collect_memory(status, root);
    collect_uptime(status, root);
    collect_network(status, root);
    collect_mounts(status, root);

    /* Compiler and workspace are payload/session facts, not kernel facts. */
    path_join(path, (int)sizeof(path), root, "/opt/vita-toolkit/VERSION");
    if (read_first_line(path, status->compiler, STATUS_FIELD) == 0) {
        char version[STATUS_FIELD];

        sset(version, STATUS_FIELD, status->compiler);
        snprintf(status->compiler, STATUS_FIELD, "tcc (toolkit %.60s)", version);
    } else {
        unknown(status->compiler);
    }

    path_join(path, (int)sizeof(path), root,
              "/mnt/vita-storage/vita-workbench/WORKSPACE-INFO");
    if (read_first_line(path, status->workspace, STATUS_FIELD) == 0)
        sset(status->workspace, STATUS_FIELD,
             "/mnt/vita-storage/vita-workbench");
    else
        unknown(status->workspace);
}
