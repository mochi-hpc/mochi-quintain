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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/resource.h>

#include <json-c/json.h>
#include <mpi.h>

#include <abt.h>
#include <quintain-client.h>
#include <flock/flock-group-view.h>
#include <flock/flock-group.h>

#include "quintain-macros.h"
#include "bedrock-c-wrapper.h"

/* record up to 32 million (power of 2) samples.  This will take 256 MiB of RAM
 * per rank */
#define MAX_SAMPLES (32 * 1024 * 1024)

struct options {
    char group_file[256];
    char json_file[256];
    char output_file[256];
};

struct sample_statistics {
    double min;
    double q1;
    double median;
    double q3;
    double max;
    double mean;
    double ops_per_sec;
};

static int  parse_args(int                  argc,
                       char**               argv,
                       struct options*      opts,
                       struct json_object** json_cfg);
static void usage(void);
static int  sample_compare(const void* p1, const void* p2);
static int
local_stat(double* utime_sec, double* stime_sec, double* alltime_sec);

int main(int argc, char** argv)
{
    int                        nranks, nproviders, my_rank;
    int                        ret;
    flock_group_view_t         group_view      = FLOCK_GROUP_VIEW_INITIALIZER;
    char*                      svr_addr_str    = NULL;
    char                       proto[64]       = {0};
    char*                      svr_cfg_str_raw = NULL;
    char*                      cli_cfg_str     = NULL;
    margo_instance_id          mid             = MARGO_INSTANCE_NULL;
    quintain_client_t          qcl             = QTN_CLIENT_NULL;
    quintain_provider_handle_t qph             = QTN_PROVIDER_HANDLE_NULL;
    flock_group_handle_t       fh              = FLOCK_GROUP_HANDLE_NULL;
    bedrock_client_t           bcl             = NULL;
    flock_client_t             fcl             = FLOCK_CLIENT_NULL;
    bedrock_service_t          bsh             = NULL;
    hg_addr_t                  svr_addr        = HG_ADDR_NULL;
    struct options             opts;
    struct json_object*        json_cfg;
    int req_buffer_size, resp_buffer_size, duration_seconds, warmup_iterations,
        bulk_size;
    hg_bulk_op_t             bulk_op;
    double                   this_ts, prev_ts, start_ts;
    double*                  samples;
    int                      sample_index = 0;
    gzFile                   f            = NULL;
    char                     rank_file[300];
    int                      i;
    int                      trace_flag   = 0;
    struct sample_statistics stats        = {0};
    void*                    bulk_buffer  = NULL;
    int                      work_flags   = 0;
    struct margo_init_info   mii          = {0};
    struct json_object*      margo_config = NULL;
    struct json_object*      svr_config   = NULL;
    double                   svr_utime1, svr_stime1, svr_alltime1;
    double                   svr_utime2, svr_stime2, svr_alltime2;
    double                   svr_utime, svr_stime, svr_alltime;
    double                   cli_utime1, cli_stime1, cli_alltime1;
    double                   cli_utime2, cli_stime2, cli_alltime2;
    double                   cli_utime, cli_stime, cli_alltime;
    int                      provider_id = -1;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    ret = parse_args(argc, argv, &opts, &json_cfg);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }
    /* file name for intermediate results from this rank */
    sprintf(rank_file, "%s.%d", opts.output_file, my_rank);

    /* load the Flock group view */
    flock_return_t fret
        = flock_group_view_from_file(opts.group_file, &group_view);
    if (fret != FLOCK_SUCCESS) {
        fprintf(stderr, "Error: flock_group_view_from_file(): %d.\n", fret);
        goto err_mpi_cleanup;
    }

    /* find transport to initialize margo to match provider */
    svr_addr_str = group_view.members.data[0].address;
    provider_id  = group_view.members.data[0].provider_id;
    for (int i = 0; i < 64 && svr_addr_str[i] != ':'; ++i)
        proto[i] = svr_addr_str[i];

    /* If there is a "margo" section in the json configuration, then
     * serialize it into a string to pass to margo_init_ext().
     */
    margo_config = json_object_object_get(json_cfg, "margo");
    if (margo_config)
        mii.json_config = json_object_to_json_string_ext(
            margo_config, JSON_C_TO_STRING_PLAIN);
    mid = margo_init_ext(proto, MARGO_CLIENT_MODE, &mii);
    if (!mid) {
        fprintf(stderr, "Error: failed to initialize margo with %s protocol.\n",
                proto);
        ret = -1;
        goto err_flock_cleanup;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /* initialize a Flock client and refresh the view in case it diverges
     * from what was in the initial group file
     */
    ret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        goto err_margo_cleanup;
    }

    ret = flock_client_init(mid, ABT_POOL_NULL, &fcl);
    if (ret != FLOCK_SUCCESS) {
        fprintf(stderr, "Error: flock_client_init() failure, ret: %d.\n", ret);
        goto err_flock_cleanup;
    }

    ret = flock_group_handle_create(fcl, svr_addr, provider_id, true, &fh);
    if (ret != FLOCK_SUCCESS) {
        fprintf(stderr,
                "Error: flock_group_handle_create() failure, ret: %d.\n", ret);
        goto err_flock_cleanup;
    }

    ret = flock_group_update_view(fh, NULL);
    if (ret != FLOCK_SUCCESS) {
        fprintf(stderr, "Error: flock_group_update_view() failure, ret: %d.\n",
                ret);
        goto err_flock_cleanup;
    }

    ret = flock_group_get_view(fh, &group_view);
    if (ret != FLOCK_SUCCESS) {
        fprintf(stderr, "Error: flock_group_get_view() failure, ret: %d.\n",
                ret);
        goto err_flock_cleanup;
    }

    /* get the number of providers */
    nproviders = flock_group_view_member_count(&group_view);
    if (nproviders == 0) {
        fprintf(stderr, "Error: flock group has no members.\n");
        goto err_flock_cleanup;
    }

    ret = bedrock_client_init(mid, &bcl);
    if (ret != BEDROCK_SUCCESS) {
        fprintf(stderr, "Error: bedrock_client_init() failure.\n");
        goto err_flock_cleanup;
    }

    /* each benchmark process selects exactly one server to contact */
    svr_addr_str = group_view.members.data[my_rank % nproviders].address;
    provider_id  = group_view.members.data[my_rank % nproviders].provider_id;

    /* resolve address to target server */
    margo_addr_free(mid, svr_addr);
    ret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        goto err_flock_cleanup;
    }

    ret = bedrock_service_handle_create(bcl, svr_addr_str, 0, &bsh);
    if (ret != BEDROCK_SUCCESS) {
        fprintf(stderr, "Error: bedrock_service_handle_create() failure.\n");
        goto err_br_cleanup;
    }

    ret = quintain_client_init(mid, &qcl);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Error: quintain_client_init() failure.\n");
        goto err_br_cleanup;
    }

    provider_id
        = json_object_get_int(json_object_object_get(json_cfg, "provider_id"));
    ret = quintain_provider_handle_create(qcl, svr_addr, provider_id, &qph);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Error: quintain_provider_handle_create() failure.\n");
        goto err_qtn_cleanup;
    }

    req_buffer_size = json_object_get_int(
        json_object_object_get(json_cfg, "req_buffer_size"));
    resp_buffer_size = json_object_get_int(
        json_object_object_get(json_cfg, "resp_buffer_size"));
    duration_seconds = json_object_get_int(
        json_object_object_get(json_cfg, "duration_seconds"));
    warmup_iterations = json_object_get_int(
        json_object_object_get(json_cfg, "warmup_iterations"));
    trace_flag
        = json_object_get_boolean(json_object_object_get(json_cfg, "trace"));
    if (json_object_get_boolean(
            json_object_object_get(json_cfg, "use_server_poolset")))
        work_flags |= QTN_WORK_USE_SERVER_POOLSET;
    bulk_size
        = json_object_get_int(json_object_object_get(json_cfg, "bulk_size"));
    if (strcmp("pull", json_object_get_string(
                           json_object_object_get(json_cfg, "bulk_direction"))))
        bulk_op = HG_BULK_PULL;
    else if (strcmp("push", json_object_get_string(json_object_object_get(
                                json_cfg, "bulk_direction"))))
        bulk_op = HG_BULK_PUSH;
    else {
        fprintf(stderr,
                "Error: invalid bulk_direction parameter: %s (must be push or "
                "pull).\n",
                json_object_get_string(
                    json_object_object_get(json_cfg, "bulk_direction")));
        goto err_qtn_cleanup;
    }

    /* allocate with mmap rather than malloc just so we can use the
     * MAP_POPULATE flag to get the paging out of the way before we start
     * measurements
     */
    samples = mmap(NULL, MAX_SAMPLES * sizeof(double), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
    if (!samples) {
        perror("mmap");
        ret = -1;
        goto err_qtn_cleanup;
    }

    /* Allocate a bulk buffer (if bulk_size > 0) to reuse in all _work()
     * calls.  Note that we do not expliclitly register it for RDMA here;
     * that will be handled within the _work() call as needed.
     */
    if (bulk_size > 0) {
        bulk_buffer = malloc(bulk_size);
        if (!bulk_buffer) {
            perror("malloc");
            ret = -1;
            goto err_qtn_cleanup;
        }
    }

    /* run warm up iterations, if specified */
    for (i = 0; i < warmup_iterations; i++) {
        ret = quintain_work(qph, req_buffer_size, resp_buffer_size, bulk_size,
                            bulk_op, bulk_buffer, work_flags);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_work() failure: (%d)\n", ret);
            goto err_qtn_cleanup;
        }
    }

    /* synchronize clients to make sure they are all ready before we query
     * statistics*/
    MPI_Barrier(MPI_COMM_WORLD);

    ret = local_stat(&cli_utime1, &cli_stime1, &cli_alltime1);
    if (ret != 0) goto err_qtn_cleanup;

    if (my_rank == 0) {
        ret = quintain_stat(qph, &svr_utime1, &svr_stime1, &svr_alltime1);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_stat() failure: (%d)\n", ret);
            goto err_qtn_cleanup;
        }
    }

    /* barrier to start measurements */
    MPI_Barrier(MPI_COMM_WORLD);

    start_ts = ABT_get_wtime();
    prev_ts  = 0;

    do {
        ret = quintain_work(qph, req_buffer_size, resp_buffer_size, bulk_size,
                            bulk_op, bulk_buffer, work_flags);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_work() failure.\n");
            goto err_qtn_cleanup;
        }
        this_ts = ABT_get_wtime() - start_ts;
        /* save just the elapsed time; we can reconstruct start and end
         * timestamps later since this is a tight loop
         */
        if (sample_index < MAX_SAMPLES)
            samples[sample_index] = this_ts - prev_ts;
        prev_ts = this_ts;
        sample_index++;
    } while (this_ts < duration_seconds);

    MPI_Barrier(MPI_COMM_WORLD);

    ret = local_stat(&cli_utime2, &cli_stime2, &cli_alltime2);
    if (ret != 0) goto err_qtn_cleanup;
    cli_utime   = cli_utime2 - cli_utime1;
    cli_stime   = cli_stime2 - cli_stime1;
    cli_alltime = cli_alltime2 - cli_alltime1;

    if (my_rank == 0) {
        ret = quintain_stat(qph, &svr_utime2, &svr_stime2, &svr_alltime2);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_stat() failure: (%d)\n", ret);
            goto err_qtn_cleanup;
        }
        svr_utime   = svr_utime2 - svr_utime1;
        svr_stime   = svr_stime2 - svr_stime1;
        svr_alltime = svr_alltime2 - svr_alltime1;
    }

    /* store results */
    f = gzopen(rank_file, "w");
    if (!f) {
        fprintf(stderr, "Error opening %s\n", opts.output_file);
        goto err_qtn_cleanup;
    }

    /* have rank 0 in benchmark report configuration */
    if (my_rank == 0) {
        struct json_tokener*    tokener;
        enum json_tokener_error jerr;

        /* retrieve configuration from provider */
        svr_cfg_str_raw
            = bedrock_service_query_config(bsh, "return $__config__;");
        if (!svr_cfg_str_raw) {
            fprintf(stderr, "Error: bedrock_service_query_config() failure.\n");
            goto err_qtn_cleanup;
        }

        /* the string emitted by bedrock_service_query_config() is not
         * formatted for human readability.  Parse it in json-c and emit it
         * again with pretty options for better legibility.
         */
        tokener    = json_tokener_new();
        svr_config = json_tokener_parse_ex(tokener, svr_cfg_str_raw,
                                           strlen(svr_cfg_str_raw));
        if (!svr_config) {
            jerr = json_tokener_get_error(tokener);
            fprintf(stderr, "JSON parse error: %s",
                    json_tokener_error_desc(jerr));
            json_tokener_free(tokener);
            return -1;
        }
        json_tokener_free(tokener);

        /* retrieve local margo configuration */
        cli_cfg_str = margo_get_config(mid);

        /* parse margo config and injected into the benchmark config */
        tokener = json_tokener_new();
        margo_config
            = json_tokener_parse_ex(tokener, cli_cfg_str, strlen(cli_cfg_str));
        if (!margo_config) {
            jerr = json_tokener_get_error(tokener);
            fprintf(stderr, "JSON parse error: %s",
                    json_tokener_error_desc(jerr));
            json_tokener_free(tokener);
            return -1;
        }
        json_tokener_free(tokener);
        /* delete existing margo object, if present */
        json_object_object_del(json_cfg, "margo");
        /* add new one, derived at run time */
        json_object_object_add(json_cfg, "margo", margo_config);

        gzprintf(f, "\"quintain-provider (first of %d)\" : %s\n", nproviders,
                 json_object_to_json_string_ext(
                     svr_config,
                     JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE));
        gzprintf(f, "\"quintain-benchmark\" : %s\n",
                 json_object_to_json_string_ext(
                     json_cfg,
                     JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE));
    }

    /* if requested, report every sample */
    if (trace_flag) {
        gzprintf(f, "start_timestamp\t%f\n", start_ts);
        gzprintf(f, "# sample_trace\t<rank>\t<start>\t<end>\t<elapsed>\n");
        this_ts = 0;
        for (i = 0; i < sample_index && i < MAX_SAMPLES; i++) {
            gzprintf(f, "sample_trace\t%d\t%.9f\t%.9f\t%.9f\n", my_rank,
                     this_ts, (this_ts + samples[i]), samples[i]);
            this_ts += samples[i];
        }
    }

    /* now that samples have been written out individually in the order they
     * occurred (if requested), we can locally sort and generate some
     * statistics
     */
    /* calculate ops/s before we possibly truncate sample_index */
    stats.ops_per_sec = (double)sample_index / (double)duration_seconds;
    if (sample_index > MAX_SAMPLES) sample_index = MAX_SAMPLES;
    qsort(samples, sample_index, sizeof(double), sample_compare);
    /* there should be a lot of samples; we aren't going to bother
     * interpolating between points if there isn't a precise sample for the
     * medians or quartiles
     */
    stats.min    = samples[0];
    stats.q1     = samples[sample_index / 4];
    stats.median = samples[sample_index / 2];
    stats.q3     = samples[3 * (sample_index / 4)];
    stats.max    = samples[sample_index - 1];
    for (i = 0; i < sample_index; i++) stats.mean += samples[i];
    stats.mean /= (double)sample_index;
    gzprintf(f, "# client_mapping\t<rank>\t<svr_idx>\t<svr_addr_string>\n");
    gzprintf(f, "client_mapping\t%d\t%d\t%s\n", my_rank, my_rank % nproviders,
             svr_addr_str);
    gzprintf(f,
             "# "
             "sample_stats\t<rank>\t<min>\t<q1>\t<median>\t<q3>\t<max>\t<mean>"
             "\t<ops/s>\n");
    gzprintf(f, "sample_stats\t%d\t%.9f\t%.9f\t%.9f\t%.9f\t%.9f\t%.9f\t%.3f\n",
             my_rank, stats.min, stats.q1, stats.median, stats.q3, stats.max,
             stats.mean, stats.ops_per_sec);
    if (my_rank == 0) {
        gzprintf(
            f, "# server_stats\t<server_rank>\t<utime>\t<stime>\t<alltime>\n");
        gzprintf(f, "server_stats\t%d\t%.9f\t%.9f\t%.9f\n", 0, svr_utime,
                 svr_stime, svr_alltime);
    }
    gzprintf(f, "# client_stats\t<rank>\t<utime>\t<stime>\t<alltime>\n");
    gzprintf(f, "client_stats\t%d\t%.9f\t%.9f\t%.9f\n", my_rank, cli_utime,
             cli_stime, cli_alltime);

    if (f) {
        gzclose(f);
        f = NULL;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    /* rank 0 concatenate all of the intermediate results into a single
     * output file.  The gzip file format produces a legal gzip'd file when
     * multiple are concatenated
     */
    if (my_rank == 0) {
        int fd_rank;
        int fd;
        rename(rank_file, opts.output_file);

        fd = open(opts.output_file, O_WRONLY | O_CREAT | O_APPEND, 0660);
        if (fd < 0) {
            perror("open");
            ret = fd;
            goto err_qtn_cleanup;
        }

        for (i = 1; i < nranks; i++) {
            /* no hard errors here; if we can't get data for a rank just
             * skip it
             */
            sprintf(rank_file, "%s.%d", opts.output_file, i);
            fd_rank = open(rank_file, O_RDONLY);
            if (fd_rank > -1) {
                do {
                    ret = read(fd_rank, samples, MAX_SAMPLES * sizeof(double));
                    if (ret > 0) write(fd, samples, ret);
                } while (ret > 0);
                close(fd_rank);
            }
            unlink(rank_file);
        }
        close(fd);
    }

err_qtn_cleanup:
    if (bulk_buffer) free(bulk_buffer);
    if (svr_cfg_str_raw) free(svr_cfg_str_raw);
    if (cli_cfg_str) free(cli_cfg_str);
    if (f) gzclose(f);
    if (samples) munmap(samples, MAX_SAMPLES * sizeof(double));
    if (qph != QTN_PROVIDER_HANDLE_NULL) quintain_provider_handle_release(qph);
    if (qcl != QTN_CLIENT_NULL) quintain_client_finalize(qcl);
err_flock_cleanup:
    flock_group_view_clear(&group_view);
    if (fh != FLOCK_GROUP_HANDLE_NULL) flock_group_handle_release(fh);
    if (fcl != FLOCK_CLIENT_NULL) flock_client_finalize(fcl);
err_br_cleanup:
    if (bsh != NULL) bedrock_service_handle_destroy(bsh);
    if (bcl != NULL) bedrock_client_finalize(bcl);
err_margo_cleanup:
    if (svr_addr != HG_ADDR_NULL) margo_addr_free(mid, svr_addr);
    margo_finalize(mid);
err_mpi_cleanup:
    if (json_cfg) json_object_put(json_cfg);
    if (svr_config) json_object_put(svr_config);
    if (ret != 0)
        MPI_Abort(MPI_COMM_WORLD, -1);
    else
        MPI_Finalize();

    return ret;
}

static int parse_json(const char* json_file, struct json_object** json_cfg)
{
    struct json_tokener*    tokener;
    enum json_tokener_error jerr;
    char*                   json_cfg_str = NULL;
    FILE*                   f;
    long                    fsize;
    int                     nranks;
    struct json_object*     val;

    /* open json file */
    f = fopen(json_file, "r");
    if (!f) {
        perror("fopen");
        fprintf(stderr, "Error: could not open json file %s\n", json_file);
        return (-1);
    }

    /* check size */
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* allocate space to hold contents and read it in */
    json_cfg_str = malloc(fsize + 1);
    if (!json_cfg_str) {
        perror("malloc");
        return (-1);
    }
    fread(json_cfg_str, 1, fsize, f);
    fclose(f);
    json_cfg_str[fsize] = 0;

    /* parse json */
    tokener = json_tokener_new();
    *json_cfg
        = json_tokener_parse_ex(tokener, json_cfg_str, strlen(json_cfg_str));
    if (!(*json_cfg)) {
        jerr = json_tokener_get_error(tokener);
        fprintf(stderr, "JSON parse error: %s", json_tokener_error_desc(jerr));
        json_tokener_free(tokener);
        free(json_cfg_str);
        return -1;
    }
    json_tokener_free(tokener);
    free(json_cfg_str);

    /* validate input params or fill in defaults */
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    CONFIG_OVERRIDE_INTEGER(*json_cfg, "nranks", nranks, 1);

    /* set defaults if not present */
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "provider_id", 1, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "duration_seconds", 2, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "req_buffer_size", 128, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "resp_buffer_size", 128, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "bulk_size", 16384, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, string, "bulk_direction", "pull", val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "warmup_iterations", 10, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, boolean, "use_server_poolset", 1, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, boolean, "trace", 1, val);

    return (0);
}

static int parse_args(int                  argc,
                      char**               argv,
                      struct options*      opts,
                      struct json_object** json_cfg)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "g:j:o:")) != -1) {
        switch (opt) {
        case 'g':
            ret = sscanf(optarg, "%s", opts->group_file);
            if (ret != 1) return (-1);
            break;
        case 'j':
            ret = sscanf(optarg, "%s", opts->json_file);
            if (ret != 1) return (-1);
            break;
        case 'o':
            ret = sscanf(optarg, "%s", opts->output_file);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (strlen(opts->group_file) == 0) return (-1);
    if (strlen(opts->json_file) == 0) return (-1);
    if (strlen(opts->output_file) == 0) return (-1);

    /* add .gz on to the output file name if it isn't already there */
    if ((strlen(opts->output_file) < 3)
        || (strcmp(".gz", &opts->output_file[strlen(opts->output_file) - 3])
            != 0)) {
        strcat(opts->output_file, ".gz");
    }

    return (parse_json(opts->json_file, json_cfg));
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: "
            "quintain-benchmark -g <group_file> -j <configuration json> -o "
            "<output file>\n");
    return;
}

static int sample_compare(const void* p1, const void* p2)
{
    double d1 = *((double*)p1);
    double d2 = *((double*)p2);

    if (d1 > d2) return 1;
    if (d1 < d2) return -1;
    return 0;
}

static int local_stat(double* utime_sec, double* stime_sec, double* alltime_sec)
{
    struct rusage usage;
    int           ret;

    ret = getrusage(RUSAGE_SELF, &usage);
    if (ret != 0) {
        perror("getrusage");
        return (-1);
    }

    *utime_sec = (double)usage.ru_utime.tv_sec
               + (double)usage.ru_utime.tv_usec / (double)1E6L;
    *stime_sec = (double)usage.ru_stime.tv_sec
               + (double)usage.ru_utime.tv_usec / (double)1E6L;
    *alltime_sec = *utime_sec + *stime_sec;

    return (0);
}
