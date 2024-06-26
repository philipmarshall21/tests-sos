/* Copyright (C) 2024 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <unistd.h>
#include <float.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <shmem.h>

#include <iostream>

// Sizes are in bytes
#define RUN_HOST_INITIATIED true

#define MSG_SIZE_MIN   1
#define MSG_SIZE_MAX   8 * 1024 * 1024

#define LARGE_MSG_SIZE 65536

#define WARMUP         100
#define WARMUP_LARGE   10

#define TRIALS         1000
#define TRIALS_LARGE   100

#define RUN_RMA_TEST_HOST(uut)                                                             \
    do {                                                                                   \
        size_t warmup_iters = (sz >= LARGE_MSG_SIZE) ? WARMUP_LARGE : WARMUP;              \
        size_t iters = (sz >= LARGE_MSG_SIZE) ? TRIALS_LARGE : TRIALS;                     \
        static double pe_bw_sum, bw;                                                       \
        pe_bw_sum = 0.0; bw = 0.0;                                                         \
        static double start_time, end_time, start_time_min, end_time_max;                  \
        start_time = DBL_MAX; end_time = 0.0; start_time_min = 0.0, end_time_max = 0.0;    \
        if (my_pe == 0) {                                                                  \
            int dest_pe = 1;                                                               \
            for (int i = 0; i < warmup_iters; i++) {                                       \
                uut(dest, src, sz, dest_pe);                                               \
                dest_pe = (dest_pe == (npes - 1)) ? dest_pe - (npes - 2) : dest_pe + 1;    \
            }                                                                              \
            shmem_barrier_all();                                                           \
                                                                                           \
            /* Timed iterations */                                                         \
            struct timeval start, end;                                                     \
            gettimeofday(&start, NULL);                                                    \
            for (int i = 0; i < iters; i++) {                                              \
                uut(dest, src, sz, dest_pe);                                               \
                dest_pe = (dest_pe == (npes - 1)) ? dest_pe - (npes - 2) : dest_pe + 1;    \
            }                                                                              \
            shmem_quiet();                                                                 \
            gettimeofday(&end, NULL);                                                      \
                                                                                           \
            /* Calculate start/end/total times in microseconds */                          \
            double start_time = start.tv_sec * 1e6;                                        \
            start_time += (double) start.tv_usec;                                          \
            double end_time = end.tv_sec * 1e6;                                            \
            end_time += (double) end.tv_usec;                                              \
                                                                                           \
            /*shmem_double_min_reduce(SHMEM_TEAM_WORLD, &start_time_min, &start_time, 1);    \
            shmem_sync_all();                                                              \
            shmem_double_max_reduce(SHMEM_TEAM_WORLD, &end_time_max, &end_time, 1);        \
            double wtime = end_time_max - start_time_min;*/                                  \
            double wtime = end_time - start_time;                                          \
                                                                                           \
            /* Calculate BW in MBs/second (1e6 bytes) */                                   \
            bw = ((sz * iters) / (wtime / 1e6)) / 1e6;                                     \
                                                                                           \
            if (my_pe == 0) {                                                              \
                printf("Size: %zu, BW: %f MB/s\n", sz, bw);                                \
            }                                                                              \
        } else {                                                                           \
            shmem_barrier_all();                                                           \
            /*shmem_double_min_reduce(SHMEM_TEAM_WORLD, &start_time_min, &start_time, 1);    \
            shmem_sync_all();                                                              \
            shmem_double_max_reduce(SHMEM_TEAM_WORLD, &end_time_max, &end_time, 1);*/        \
        }                                                                                  \
    } while (0)

int main(int argc, char **argv)
{
    shmem_init();
    
    int my_pe = shmem_my_pe();
    int npes = shmem_n_pes();

    char *dest = (char *) shmem_malloc(MSG_SIZE_MAX);
    char *src = (char *) shmem_malloc(MSG_SIZE_MAX);
    shmem_barrier_all();
    for (size_t sz = MSG_SIZE_MIN; sz <= MSG_SIZE_MAX; sz *= 2) {
        RUN_RMA_TEST_HOST(shmem_char_get_nbi);
    }

    shmem_finalize();
    return 0;
}
