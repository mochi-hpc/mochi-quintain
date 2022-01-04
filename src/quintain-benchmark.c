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

#include <json-c/json.h>
#include <mpi.h>

#include <abt.h>
#include <ssg.h>
#include <quintain-client.h>

/* record up to 32 million (power of 2) samples.  This will take 256 MiB of RAM
 * per rank */
#define MAX_SAMPLES (32 * 1024 * 1024)

// Overrides a field with an integer. If the field already existed and was
// different from the new value, and __warning is true, prints a warning.
#define CONFIG_OVERRIDE_INTEGER(__config, __key, __value, __warning)          \
    do {                                                                      \
        struct json_object* _tmp = json_object_object_get(__config, __key);   \
        if (_tmp && __warning) {                                              \
            if (!json_object_is_type(_tmp, json_type_int))                    \
                fprintf(stderr, "Overriding field \"%s\" with value %d",      \
                        __key, (int)__value);                                 \
            else if (json_object_get_int(_tmp) != __value)                    \
                fprintf(stderr, "Overriding field \"%s\" (%d) with value %d", \
                        __key, json_object_get_int(_tmp), __value);           \
        }                                                                     \
        json_object_object_add(__config, __key,                               \
                               json_object_new_int64(__value));               \
    } while (0)

// Checks if a JSON object has a particular key and its value is of the
// specified type (not array or object or null). If the field does not exist,
// creates it with the provided value.. If the field exists but is not of type
// object, prints an error and return -1. After a call to this macro, __out is
// set to the ceated/found field.
#define CONFIG_HAS_OR_CREATE(__config, __type, __key, __value, __out)    \
    do {                                                                 \
        __out = json_object_object_get(__config, __key);                 \
        if (__out && !json_object_is_type(__out, json_type_##__type)) {  \
            fprintf(stderr,                                              \
                    "\"%s\" in configuration but has an incorrect type " \
                    "(expected %s)",                                     \
                    __key, #__type);                                     \
            return -1;                                                   \
        }                                                                \
        if (!__out) {                                                    \
            __out = json_object_new_##__type(__value);                   \
            json_object_object_add(__config, __key, __out);              \
        }                                                                \
    } while (0)

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
};

static int  parse_args(int                  argc,
                       char**               argv,
                       struct options*      opts,
                       struct json_object** json_cfg);
static void usage(void);
static int  sample_compare(const void* p1, const void* p2);

int main(int argc, char** argv)
{
    int                        nranks, nproviders, my_rank;
    int                        ret;
    ssg_group_id_t             gid;
    char*                      svr_addr_str = NULL;
    char                       proto[64]    = {0};
    char*                      svr_cfg_str  = NULL;
    char*                      cli_cfg_str  = NULL;
    margo_instance_id          mid          = MARGO_INSTANCE_NULL;
    quintain_client_t          qcl          = QTN_CLIENT_NULL;
    quintain_provider_handle_t qph          = QTN_PROVIDER_HANDLE_NULL;
    hg_addr_t                  svr_addr     = HG_ADDR_NULL;
    struct options             opts;
    struct json_object*        json_cfg;
    int req_buffer_size, resp_buffer_size, duration_seconds, warmup_iterations;
    double                   this_ts, prev_ts, start_ts;
    double*                  samples;
    int                      sample_index = 0;
    gzFile                   f            = NULL;
    char                     rank_file[300];
    int                      i;
    int                      trace_flag = 0;
    struct sample_statistics stats      = {0};
    ssg_member_id_t          svr_id;

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

    /* find transport to initialize margo to match provider */
    ret = ssg_get_group_transport_from_file(opts.group_file, proto, 64);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: ssg_get_group_transport_from_file(): %s.\n",
                ssg_strerror(ret));
        goto err_mpi_cleanup;
    }

    mid = margo_init(proto, MARGO_CLIENT_MODE, 0, 0);
    if (!mid) {
        fprintf(stderr, "Error: failed to initialize margo with %s protocol.\n",
                proto);
        ret = -1;
        goto err_mpi_cleanup;
    }

    ret = ssg_init();
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: ssg_init(): %s.\n", ssg_strerror(ret));
        goto err_margo_cleanup;
    }

    /* load ssg group information */
    nproviders = 1;
    ret        = ssg_group_id_load(opts.group_file, &nproviders, &gid);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: ssg_group_id_load(): %s.\n", ssg_strerror(ret));
        goto err_ssg_cleanup;
    }

    /* get addr for rank 0 in ssg group */
    ret = ssg_get_group_member_id_from_rank(gid, 0, &svr_id);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: ssg_group_member_id_from_rank(): %s.\n",
                ssg_strerror(ret));
        goto err_ssg_cleanup;
    }

    ret = ssg_get_group_member_addr_str(gid, svr_id, &svr_addr_str);
    if (ret != SSG_SUCCESS) {
        fprintf(stderr, "Error: ssg_get_group_member_addr_str(): %s.\n",
                ssg_strerror(ret));
        goto err_ssg_cleanup;
    }

    ret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        goto err_ssg_cleanup;
    }

    ret = quintain_client_init(mid, &qcl);
    if (ret != QTN_SUCCESS) {
        fprintf(stderr, "Error: quintain_client_init() failure.\n");
        goto err_ssg_cleanup;
    }

    /* TODO: allow other provider_id values besides 1 */
    ret = quintain_provider_handle_create(qcl, svr_addr, 1, &qph);
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

    /* run warm up iterations, if specified */
    for (i = 0; i < warmup_iterations; i++) {
        ret = quintain_work(qph, req_buffer_size, resp_buffer_size);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_work() failure.\n");
            goto err_qtn_cleanup;
        }
    }

    /* barrier to start measurements */
    MPI_Barrier(MPI_COMM_WORLD);

    start_ts = ABT_get_wtime();
    prev_ts  = 0;

    do {
        ret = quintain_work(qph, req_buffer_size, resp_buffer_size);
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

    /* store results */
    f = gzopen(rank_file, "w");
    if (!f) {
        fprintf(stderr, "Error opening %s\n", opts.output_file);
        goto err_qtn_cleanup;
    }

    /* have rank 0 in benchmark report configuration */
    if (my_rank == 0) {
        /* retrieve configuration from provider */
        ret = quintain_get_server_config(qph, &svr_cfg_str);
        if (ret != QTN_SUCCESS) {
            fprintf(stderr, "Error: quintain_get_server_config() failure.\n");
            goto err_qtn_cleanup;
        }

        /* retrieve local configuration */
        cli_cfg_str = margo_get_config(mid);

        gzprintf(f, "\"margo (server)\" : %s\n", svr_cfg_str);
        gzprintf(f, "\"margo (client)\" : %s\n", cli_cfg_str);
        gzprintf(f, "\"quintain-benchmark\" : %s\n",
                 json_object_to_json_string_ext(
                     json_cfg,
                     JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE));
    }

    /* if requested, report every sample */
    if (trace_flag) {
        gzprintf(f, "# sample_trace\t<rank>\t<start>\t<end>\t<elapsed>\n");
        this_ts = 0;
        for (i = 0; i < sample_index && i < MAX_SAMPLES; i++) {
            gzprintf(f, "sample_trace\t%d\t%f\t%f\t%f\n", my_rank, this_ts,
                     (this_ts + samples[i]), samples[i]);
            this_ts += samples[i];
        }
    }

    /* now that samples have been written out individually in the order they
     * occurred (if requested), we can locally sort and generate some
     * statistics
     */
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
    gzprintf(
        f,
        "# sample_stats\t<rank>\t<min>\t<q1>\t<median>\t<q3>\t<max>\t<mean>\n");
    gzprintf(f, "sample_stats\t%d\t%f\t%f\t%f\t%f\t%f\t%f\n", my_rank,
             stats.min, stats.q1, stats.median, stats.q3, stats.max,
             stats.mean);

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
    if (svr_cfg_str) free(svr_cfg_str);
    if (cli_cfg_str) free(cli_cfg_str);
    if (f) gzclose(f);
    if (samples) munmap(samples, MAX_SAMPLES * sizeof(double));
    if (qph != QTN_PROVIDER_HANDLE_NULL) quintain_provider_handle_release(qph);
    if (qcl != QTN_CLIENT_NULL) quintain_client_finalize(qcl);
err_ssg_cleanup:
    if (svr_addr_str) free(svr_addr_str);
    ssg_finalize();
err_margo_cleanup:
    if (svr_addr != HG_ADDR_NULL) margo_addr_free(mid, svr_addr);
    margo_finalize(mid);
err_mpi_cleanup:
    if (json_cfg) json_object_put(json_cfg);
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
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "duration_seconds", 2, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "req_buffer_size", 128, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "resp_buffer_size", 128, val);
    CONFIG_HAS_OR_CREATE(*json_cfg, int, "warmup_iterations", 10, val);
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
