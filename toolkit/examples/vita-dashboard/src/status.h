#ifndef VITA_DASHBOARD_STATUS_H
#define VITA_DASHBOARD_STATUS_H

#define STATUS_FIELD 96

/* Read-only system facts, gathered from /proc and /sys only. */
struct dashboard_status {
    char kernel[STATUS_FIELD];
    char arch[STATUS_FIELD];
    char cpus_online[STATUS_FIELD];
    char memory_available[STATUS_FIELD];
    char uptime[STATUS_FIELD];
    char interface[STATUS_FIELD];
    char address[STATUS_FIELD];
    char storage[STATUS_FIELD];
    char toolkit[STATUS_FIELD];
    char compiler[STATUS_FIELD];
    char workspace[STATUS_FIELD];
};

/* Every field is filled, using "UNKNOWN" when unavailable. Never fails. */
void status_collect(struct dashboard_status *status, const char *root);

#endif /* VITA_DASHBOARD_STATUS_H */
