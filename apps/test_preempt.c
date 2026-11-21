#include "uthread.h"
#include <stdint.h>
#include <stdio.h>

static volatile uint64_t sink = 0;

static void worker(void *arg)
{
    const char c = (char)(uintptr_t)arg;

    for (int k = 0; k < 20; k++) {

        for (uint64_t i = 0; i < 5000000; i++) {
            sink += (i ^ (uint64_t)c);
        }
        putchar(c);
        fflush(stdout);
    }
    putchar('\n');
}

void test_two_workers(void *arg)
{
    (void)arg;
    uthread_create(worker, (void *)(uintptr_t)'A');
    uthread_create(worker, (void *)(uintptr_t)'E');
}

int main(void)
{
    uthread_run(true, test_two_workers, NULL);
    return 0;
}
