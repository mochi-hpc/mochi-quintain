/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include "mochi-quintain-config.h"

#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <margo.h>
#include <margo-bulk-pool.h>
#include <quintain-server.h>

#include "quintain-rpc.h"
#include "quintain-macros.h"

DECLARE_MARGO_RPC_HANDLER(qtn_work_ult)

static int validate_and_complete_config(struct json_object* _config,
                                        ABT_pool            _progress_pool);
static int setup_poolset(quintain_provider_t provider);

struct quintain_provider {
    margo_instance_id mid;
    ABT_pool handler_pool; // pool used to run RPC handlers for this provider
    margo_bulk_poolset_t poolset; /* intermediate buffers, if used */

    hg_id_t qtn_work_rpc_id;

    struct json_object* json_cfg;
};

static void quintain_server_finalize_cb(void* data)
{
    struct quintain_provider* provider = (struct quintain_provider*)data;
    assert(provider);

    margo_deregister(provider->mid, provider->qtn_work_rpc_id);

    if (provider->poolset) margo_bulk_poolset_destroy(provider->poolset);

    if (provider->json_cfg) json_object_put(provider->json_cfg);

    free(provider);
    return;
}

int quintain_provider_register(margo_instance_id mid,
                               uint16_t          provider_id,
                               const struct quintain_provider_init_info* uargs,
                               quintain_provider_t* provider)
{
    struct quintain_provider_init_info args         = *uargs;
    struct quintain_provider*          tmp_provider = NULL;
    int                                ret;
    hg_id_t                            rpc_id;
    struct json_object*                config = NULL;

    /* check if a provider with the same provider id already exists */
    {
        hg_id_t   id;
        hg_bool_t flag;
        margo_provider_registered_name(mid, "qtn_work_rpc", provider_id, &id,
                                       &flag);
        if (flag == HG_TRUE) {
            QTN_ERROR(mid,
                      "quintain_provider_register(): a quintain provider with "
                      "the same "
                      "id (%d) already exists",
                      provider_id);
            ret = QTN_ERR_MERCURY;
            goto error;
        }
    }

    if (args.json_config && strlen(args.json_config) > 0) {
        /* read JSON config from provided string argument */
        struct json_tokener*    tokener = json_tokener_new();
        enum json_tokener_error jerr;

        config = json_tokener_parse_ex(tokener, args.json_config,
                                       strlen(args.json_config));
        if (!config) {
            jerr = json_tokener_get_error(tokener);
            QTN_ERROR(mid, "JSON parse error: %s",
                      json_tokener_error_desc(jerr));
            json_tokener_free(tokener);
            ret = QTN_ERR_INVALID_ARG;
            goto error;
        }
        json_tokener_free(tokener);
    } else {
        /* create default JSON config */
        config = json_object_new_object();
    }

    /* validate and complete configuration */
    ret = validate_and_complete_config(config, args.rpc_pool);
    if (ret != 0) {
        QTN_ERROR(mid, "could not validate and complete configuration");
        ret = QTN_ERR_INVALID_ARG;
        goto error;
    }

    /* allocate the resulting structure */
    tmp_provider = calloc(1, sizeof(*tmp_provider));
    if (!tmp_provider) {
        ret = QTN_ERR_ALLOCATION;
        goto error;
    }
    tmp_provider->json_cfg = config;
    tmp_provider->mid      = mid;

    if (args.rpc_pool != NULL)
        tmp_provider->handler_pool = args.rpc_pool;
    else
        margo_get_handler_pool(mid, &(tmp_provider->handler_pool));

    /* create buffer poolset if needed for config */
    ret = setup_poolset(tmp_provider);
    if (ret != 0) {
        QTN_ERROR(mid, "could not create poolset");
        goto error;
    }

    /* register RPCs */
    rpc_id = MARGO_REGISTER_PROVIDER(mid, "qtn_work_rpc", qtn_work_in_t,
                                     qtn_work_out_t, qtn_work_ult, provider_id,
                                     tmp_provider->handler_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->qtn_work_rpc_id = rpc_id;

    /* install the quintain server finalize callback */
    margo_provider_push_finalize_callback(
        mid, tmp_provider, &quintain_server_finalize_cb, tmp_provider);

    if (provider != QTN_PROVIDER_IGNORE) *provider = tmp_provider;

    return QTN_SUCCESS;

error:

    if (config) json_object_put(config);
    if (tmp_provider) {
        if (tmp_provider->poolset)
            margo_bulk_poolset_destroy(tmp_provider->poolset);
        free(tmp_provider);
    }

    return (ret);
}

int quintain_provider_deregister(quintain_provider_t provider)
{
    margo_provider_pop_finalize_callback(provider->mid, provider);
    quintain_server_finalize_cb(provider);
    return QTN_SUCCESS;
}

static void qtn_work_ult(hg_handle_t handle)
{
    margo_instance_id     mid = MARGO_INSTANCE_NULL;
    qtn_work_in_t         in;
    qtn_work_out_t        out;
    const struct hg_info* info     = NULL;
    quintain_provider_t   provider = NULL;
    hg_return_t           hret;
    void*                 bulk_buffer = NULL;
    int                   bulk_flag   = HG_BULK_WRITE_ONLY;
    hg_bulk_t             bulk_handle = HG_BULK_NULL;

    memset(&out, 0, sizeof(out));

    mid = margo_hg_handle_get_instance(handle);
    assert(mid);
    info     = margo_get_info(handle);
    provider = margo_registered_data(mid, info->id);
    if (!provider) {
        out.ret = QTN_ERR_UNKNOWN_PROVIDER;
        QTN_ERROR(mid, "Unkown provider");
        goto finish;
    }

    hret = margo_get_input(handle, &in);
    if (hret != HG_SUCCESS) {
        out.ret = QTN_ERR_MERCURY;
        QTN_ERROR(mid, "margo_get_input: %s", HG_Error_to_string(hret));
        goto finish;
    }

    out.resp_buffer_size = in.resp_buffer_size;
    if (in.resp_buffer_size)
        out.resp_buffer = calloc(1, in.resp_buffer_size);
    else
        out.resp_buffer = NULL;

    if (in.bulk_size) {
        /* we were asked to perform a bulk transfer */
        if (in.flags & QTN_WORK_USE_SERVER_POOLSET) {
            /* get buffer from poolset */
            out.ret = margo_bulk_poolset_get(provider->poolset, in.bulk_size,
                                             &bulk_handle);
            if (out.ret != 0) {
                out.ret = QTN_ERR_ALLOCATION;
                QTN_ERROR(mid, "margo_bulk_poolset_get: %s",
                          HG_Error_to_string(hret));
                goto finish;
            }
        } else {
            /* allocate buffer and register */
            bulk_buffer = malloc(in.bulk_size);
            if (!bulk_buffer) {
                out.ret = QTN_ERR_ALLOCATION;
                goto finish;
            }
            if (in.bulk_op == HG_BULK_PUSH) bulk_flag = HG_BULK_READ_ONLY;
            out.ret = margo_bulk_create(mid, 1, (void**)(&bulk_buffer),
                                        &in.bulk_size, bulk_flag, &bulk_handle);
            if (out.ret != HG_SUCCESS) {
                QTN_ERROR(mid, "margo_bulk_poolset_get: %s",
                          HG_Error_to_string(hret));
                goto finish;
            }
        }

        /* transfer */
        out.ret
            = margo_bulk_transfer(mid, in.bulk_op, info->addr, in.bulk_handle,
                                  0, bulk_handle, 0, in.bulk_size);
    }
    if (out.ret != HG_SUCCESS) {
        QTN_ERROR(mid, "margo_bulk_transfer: %s", HG_Error_to_string(out.ret));
    }

finish:
    margo_respond(handle, &out);
    margo_free_input(handle, &in);
    if (bulk_handle != HG_BULK_NULL) {
        if (in.flags & QTN_WORK_USE_SERVER_POOLSET)
            margo_bulk_poolset_release(provider->poolset, bulk_handle);
        else
            margo_bulk_free(bulk_handle);
    }
    if (bulk_buffer != NULL) free(bulk_buffer);
    if (out.resp_buffer) free(out.resp_buffer);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(qtn_work_ult)

static int validate_and_complete_config(struct json_object* _config,
                                        ABT_pool            _progress_pool)
{
    struct json_object* val;
    long                page_size;

    /* report version number for this component */
    CONFIG_OVERRIDE_STRING(_config, "version", PACKAGE_VERSION, "version", 1);

    /* populate default poolset settings if not specified already */

    /* poolset yes or no; implies intermediate buffering */
    CONFIG_HAS_OR_CREATE(_config, boolean, "poolset_enable", 1, val);
    /* number of preallocated buffer pools */
    CONFIG_HAS_OR_CREATE(_config, int64, "poolset_npools", 4, val);
    /* buffers per buffer pool */
    CONFIG_HAS_OR_CREATE(_config, int64, "poolset_nbuffers_per_pool", 32, val);
    /* size of buffers in smallest pool */
    CONFIG_HAS_OR_CREATE(_config, int64, "poolset_first_buffer_size", 65536,
                         val);
    /* factor size increase per pool */
    CONFIG_HAS_OR_CREATE(_config, int64, "poolset_multiplier", 4, val);

    /* retrieve system page size (this can only be queried, not set by
     * caller
     */
    page_size = sysconf(_SC_PAGESIZE);
    CONFIG_OVERRIDE_INTEGER(_config, "page_size", (int)page_size, 1);

    return (0);
}

char* quintain_provider_get_config(quintain_provider_t provider)
{
    struct rusage usage;
    int           ret;

    /* update maxrss on demand */
    ret = getrusage(RUSAGE_SELF, &usage);
    if (ret != 0) {
        /* if we can't get the current rusage, then delete it from json if
         * present so that we don't report an incorrect value.
         */
        json_object_object_del(provider->json_cfg, "ru_maxrss");
    } else {
        CONFIG_OVERRIDE_INTEGER(provider->json_cfg, "ru_maxrss",
                                usage.ru_maxrss, 0);
    }

    const char* content = json_object_to_json_string_ext(
        provider->json_cfg,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    return strdup(content);
}

static int setup_poolset(quintain_provider_t provider)
{
    hg_return_t hret;

    /* NOTE: this is called after validate, so we don't need extensive error
     * checking on the json here
     */

    /* create poolset if we don't have one yet */
    if (provider->poolset == NULL
        && json_object_get_boolean(
            json_object_object_get(provider->json_cfg, "poolset_enable"))) {
        hret = margo_bulk_poolset_create(
            provider->mid,
            json_object_get_int(
                json_object_object_get(provider->json_cfg, "poolset_npools")),
            json_object_get_int(json_object_object_get(
                provider->json_cfg, "poolset_nbuffers_per_pool")),
            json_object_get_int(json_object_object_get(
                provider->json_cfg, "poolset_first_buffer_size")),
            json_object_get_int(json_object_object_get(provider->json_cfg,
                                                       "poolset_multiplier")),
            HG_BULK_READWRITE, &(provider->poolset));
        if (hret != 0) return QTN_ERR_MERCURY;
    }

    /* destroy poolset if we have one but it has been disabled */
    if (provider->poolset
        && !json_object_get_boolean(
            json_object_object_get(provider->json_cfg, "poolset_enable"))) {
        hret = margo_bulk_poolset_destroy(provider->poolset);
        if (hret != 0) return QTN_ERR_MERCURY;
    }

    /* otherwise nothing to do here */
    return QTN_SUCCESS;
}
