#ifndef VITA_DASHBOARD_INPUT_H
#define VITA_DASHBOARD_INPUT_H

#define INPUT_TEXT 256

enum dashboard_action {
    DASHBOARD_ACTION_NONE = 0,
    DASHBOARD_ACTION_NEXT,
    DASHBOARD_ACTION_EXIT
};

struct dashboard_input {
    int fd;
    unsigned int number;
    char name[INPUT_TEXT];
    char phys[INPUT_TEXT];
    char path[INPUT_TEXT];
    unsigned long events_seen;
    unsigned long malformed;
};

/*
 * Select a device BY IDENTITY (the sysfs `phys` string), never by a hardcoded
 * event number.  Returns 0 on success.  Opens read-only and non-blocking.
 */
int input_open(struct dashboard_input *in, const char *sysroot,
               const char *devroot, const char *wanted_phys);

/*
 * Poll for up to timeout_ms.  A timeout is a NORMAL bounded result, reported as
 * DASHBOARD_ACTION_NONE with a 0 return.  Returns -1 only on a real error.
 */
int input_poll(struct dashboard_input *in, int timeout_ms,
               enum dashboard_action *action);

void input_close(struct dashboard_input *in);

#endif /* VITA_DASHBOARD_INPUT_H */
