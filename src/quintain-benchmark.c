/*
 * Copyright (c) 2021 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include "mochi-quintain-config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mpi.h>

struct options {};
static struct options g_opts;

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

int main(int argc, char** argv)
{
    int nranks;
    int namelen;
    int ret;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
        default:
            return (-1);
        }
    }

    return (0);
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: "
            "quintain-benchmark\n");
    return;
}
