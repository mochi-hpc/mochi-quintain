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
#include <quintain-client.h>

struct options {
    char group_file[256];
};
static struct options g_opts;

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

/* TODO: where should this function reside?  This is copied from
 * bv-client.cc but it is probably more generally useful for a client that
 * bootstraps based on a generic ssg group id in which it doesn't know which
 * protocol to start margo with otherwise
 *
 * Note that this version returns a pointer that must be free'd by caller
 */
static char* get_proto_from_addr(char* addr_str)
{
    char* p = strdup(addr_str);
    if (p == NULL) return NULL;
    char* q = strchr(p, ':');
    if (q == NULL) {
        free(p);
        return NULL;
    }
    *q = '\0';
    return p;
}

int main(int argc, char** argv)
{
    int                        nranks, nproviders;
    int                        ret;
    ssg_group_id_t             gid;
    char*                      svr_addr_str = NULL;
    char*                      proto;
    char*                      svr_cfg_str;
    margo_instance_id          mid      = MARGO_INSTANCE_NULL;
    quintain_client_t          qcl      = QTN_CLIENT_NULL;
    quintain_provider_handle_t qph      = QTN_PROVIDER_HANDLE_NULL;
    hg_addr_t                  svr_addr = HG_ADDR_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* TODO: update to show human readable errors on ssg failures once SSG
     * api has error printing macro
     */

    ret = ssg_init();
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize ssg.\n");
        goto err_mpi_cleanup;
    }

    /* load ssg group information */
    nproviders = 1;
    ret        = ssg_group_id_load(g_opts.group_file, &nproviders, &gid);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: failed to load ssg group from file %s.\n",
                g_opts.group_file);
        goto err_ssg_cleanup;
    }

    /* get addr for rank 0 in ssg group */
    ret = ssg_group_id_get_addr_str(gid, 0, &svr_addr_str);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr,
                "Error: failed to retrieve first server addr from ssg.\n");
        goto err_ssg_cleanup;
    }

    printf("DBG: svr_addr_str: %s\n", svr_addr_str);

    /* find protocol */
    proto = get_proto_from_addr(svr_addr_str);
    /* this should never fail if addr is properly formatted */
    assert(proto);

    mid = margo_init(proto, MARGO_CLIENT_MODE, 0, 0);
    free(proto);
    if (!mid) {
        fprintf(stderr, "Error: failed to initialize margo with %s protocol.\n",
                proto);
        ret = -1;
        goto err_ssg_cleanup;
    }

    ret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        goto err_margo_cleanup;
    }

    /* TODO: error code printing fn for quintain */
    ret = quintain_client_init(mid, &qcl);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Error: quintain_client_init() failure.\n");
        goto err_margo_cleanup;
    }

    /* TODO: allow other provider_id values besides 1 */
    ret = quintain_provider_handle_create(qcl, svr_addr, 1, &qph);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Error: quintain_provider_handle_create() failure.\n");
        goto err_qtn_cleanup;
    }

    /* retrieve configuration from provider so that we can report it with
     * results
     */
    ret = quintain_get_server_config(qph, &svr_cfg_str);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Erroro: quintain_get_server_config() failure.\n");
        goto err_qtn_cleanup;
    }

    printf("DBG: svr_cfg_str: %s\n", svr_cfg_str);
    if (svr_cfg_str) free(svr_cfg_str);

err_qtn_cleanup:
    if (qph != QTN_PROVIDER_HANDLE_NULL) quintain_provider_handle_release(qph);
    if (qcl != QTN_CLIENT_NULL) quintain_client_finalize(qcl);
err_margo_cleanup:
    if (svr_addr != HG_ADDR_NULL) margo_addr_free(mid, svr_addr);
    margo_finalize(mid);
err_ssg_cleanup:
    if (svr_addr_str) free(svr_addr_str);
    ssg_finalize();
err_mpi_cleanup:
    MPI_Finalize();

    return ret;
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
