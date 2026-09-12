/*
 * hello-native — the reference two-translation-unit native build for the
 * Vita Linux Workbench.
 *
 * Exercises: two translation units, target sysroot headers, one pthread
 * worker, one libm call, explicit stdout flushing, and a deterministic exit
 * status.  It reads nothing outside /proc and writes nothing.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"

struct worker_arg {
    unsigned long iterations;
    double accumulator;
};

static void *worker_main(void *raw)
{
    struct worker_arg *arg = raw;
    unsigned long i;

    arg->accumulator = 0.0;
    for (i = 1; i <= arg->iterations; i++)
        arg->accumulator += vita_unit_step((double)i);

    return raw;
}

int main(void)
{
    struct worker_arg arg;
    struct vita_system_info info;
    pthread_t worker;
    void *joined = NULL;
    int rc;

    memset(&arg, 0, sizeof(arg));
    arg.iterations = 1024;

    rc = pthread_create(&worker, NULL, worker_main, &arg);
    if (rc != 0) {
        fprintf(stderr, "hello-native: pthread_create failed: %d\n", rc);
        return 2;
    }

    rc = pthread_join(worker, &joined);
    if (rc != 0 || joined != &arg) {
        fprintf(stderr, "hello-native: pthread_join failed: %d\n", rc);
        return 3;
    }

    if (vita_system_info(&info) != 0) {
        fprintf(stderr, "hello-native: could not read system info\n");
        return 4;
    }

    /* The accumulator is a deterministic function of the iteration count, so
     * the expected value is checked rather than merely printed. */
    if (!vita_accumulator_is_expected(arg.accumulator, arg.iterations)) {
        fprintf(stderr, "hello-native: accumulator mismatch: %f\n",
                arg.accumulator);
        return 5;
    }

    printf("schema=1\n");
    printf("example=hello-native\n");
    printf("threads=1\n");
    printf("iterations=%lu\n", arg.iterations);
    printf("accumulator=%.4f\n", arg.accumulator);
    printf("kernel=%s\n", info.kernel);
    printf("arch=%s\n", info.arch);
    printf("cpus=%d\n", info.cpu_count);
    printf("status=ok\n");

    /* Explicit: the harness reads this output after the process exits. */
    if (fflush(stdout) != 0) {
        fprintf(stderr, "hello-native: stdout flush failed\n");
        return 6;
    }

    return 0;
}
