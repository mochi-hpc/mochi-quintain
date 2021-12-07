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

#include <ssg.h>

struct options {
    char group_file[256];
};
static struct options g_opts;

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

int main(int argc, char** argv)
{
    int            nranks, nproviders;
    int            ret;
    ssg_group_id_t group_id;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    ret = ssg_init();
    /* TODO: how to display ssg errors? */
    assert(ret == 0);

    nproviders = 1;
    ret        = ssg_group_id_load(g_opts.group_file, &nproviders, &group_id);
    /* TODO: how to display ssg errors? */
    assert(ret == 0);

    ssg_finalize();

    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "g:")) != -1) {
        switch (opt) {
        case 'g':
            ret = sscanf(optarg, "%s", opts->group_file);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (strlen(opts->group_file) == 0) return (-1);

    return (0);
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: "
            "quintain-benchmark -g <group_file>\n");
    return;
}
