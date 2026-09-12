#ifndef VITA_HELLO_NATIVE_SYSTEM_H
#define VITA_HELLO_NATIVE_SYSTEM_H

#define VITA_SYSTEM_FIELD 96

struct vita_system_info {
    char kernel[VITA_SYSTEM_FIELD];
    char arch[VITA_SYSTEM_FIELD];
    int cpu_count;
};

/* Fills @info from uname(2) and sysconf(3). Returns 0 on success. */
int vita_system_info(struct vita_system_info *info);

/* One libm call per step; separated so main.c and system.c are genuinely
 * distinct translation units that must be linked together. */
double vita_unit_step(double value);

/* Deterministic check of the accumulated sum for @iterations steps. */
int vita_accumulator_is_expected(double accumulator, unsigned long iterations);

#endif /* VITA_HELLO_NATIVE_SYSTEM_H */
