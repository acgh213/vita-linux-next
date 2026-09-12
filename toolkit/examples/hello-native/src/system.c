/*
 * system.c — the second translation unit of hello-native.
 *
 * Uses target sysroot headers (sys/utsname.h, unistd.h) and one libm call.
 */
#include <math.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "system.h"

int vita_system_info(struct vita_system_info *info)
{
    struct utsname buffer;
    long cpus;

    if (info == NULL)
        return -1;

    memset(info, 0, sizeof(*info));

    if (uname(&buffer) != 0)
        return -1;

    strncpy(info->kernel, buffer.release, VITA_SYSTEM_FIELD - 1);
    strncpy(info->arch, buffer.machine, VITA_SYSTEM_FIELD - 1);

    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    info->cpu_count = (cpus > 0) ? (int)cpus : 0;

    return 0;
}

double vita_unit_step(double value)
{
    /* sqrt(x)^2 / x == 1.0 for every positive x, so the sum over n steps is
     * exactly n.  This gives a libm dependency with a deterministic result. */
    double root = sqrt(value);

    return (root * root) / value;
}

int vita_accumulator_is_expected(double accumulator, unsigned long iterations)
{
    double expected = (double)iterations;
    double delta = accumulator - expected;

    if (delta < 0.0)
        delta = -delta;

    /* Tolerance scales with the operand count to stay meaningful for larger
     * iteration counts without ever accepting a structurally wrong sum. */
    return delta <= (1e-9 * expected);
}
