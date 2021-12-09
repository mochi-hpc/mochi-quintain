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
    ssg_group_id_t gid;
    char*          svr_addr_str;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* TODO: revisit these assertions and see if we can print a human
     * readable error once an error printing macro is available in the SSG
     * API
     */

    ret = ssg_init();
    assert(ret == 0);

    /* load ssg group information */
    nproviders = 1;
    ret        = ssg_group_id_load(g_opts.group_file, &nproviders, &gid);
    assert(ret == 0);

    /* get addr for rank 0 in ssg group */
    ret = ssg_group_id_get_addr_str(gid, 0, &svr_addr_str);
    assert(ret == 0);

    printf("DBG: svr_addr_str: %s\n", svr_addr_str);

    /* TODO: pick back up here.  Strip prefix, start margo */

    free(svr_addr_str);

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
