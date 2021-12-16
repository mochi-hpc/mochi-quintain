/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <assert.h>

#include <margo.h>
#include <quintain-server.h>

#include "quintain-rpc.h"

DECLARE_MARGO_RPC_HANDLER(qtn_get_server_config_ult)

struct quintain_provider {
    margo_instance_id mid;
    ABT_pool handler_pool; // pool used to run RPC handlers for this provider

    hg_id_t qtn_get_server_config_rpc_id;

    struct json_object* json_cfg;
};

static void quintain_server_finalize_cb(void* data)
{
    struct quintain_provider* provider = (struct quintain_provider*)data;
    assert(provider);

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
    struct json_object*                config = NULL;
    hg_id_t                            rpc_id;

    /* check if a provider with the same provider id already exists */
    {
        hg_id_t   id;
        hg_bool_t flag;
        margo_provider_registered_name(mid, "quintain_get_server_config_rpc",
                                       provider_id, &id, &flag);
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

    /* allocate the resulting structure */
    tmp_provider = calloc(1, sizeof(*tmp_provider));
    if (!tmp_provider) {
        ret = QTN_ERR_ALLOCATION;
        goto error;
    }
    tmp_provider->mid = mid;

    if (args.rpc_pool != NULL)
        tmp_provider->handler_pool = args.rpc_pool;
    else
        margo_get_handler_pool(mid, &(tmp_provider->handler_pool));

    /* register RPCs */
    rpc_id = MARGO_REGISTER_PROVIDER(
        mid, "qtn_get_server_config_rpc", void, qtn_get_server_config_out_t,
        qtn_get_server_config_ult, provider_id, tmp_provider->handler_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->qtn_get_server_config_rpc_id = rpc_id;

    /* install the quintain server finalize callback */
    margo_provider_push_finalize_callback(
        mid, tmp_provider, &quintain_server_finalize_cb, tmp_provider);

    if (provider != QTN_PROVIDER_IGNORE) *provider = tmp_provider;

    return QTN_SUCCESS;

error:

    if (tmp_provider) free(tmp_provider);

    return (ret);
}

int quintain_provider_deregister(quintain_provider_t provider)
{
    margo_provider_pop_finalize_callback(provider->mid, provider);
    quintain_server_finalize_cb(provider);
    return QTN_SUCCESS;
}

static void qtn_get_server_config_ult(hg_handle_t handle)
{
    margo_instance_id           mid = MARGO_INSTANCE_NULL;
    qtn_get_server_config_out_t out;
    const struct hg_info*       info     = NULL;
    quintain_provider_t         provider = NULL;

    mid = margo_hg_handle_get_instance(handle);
    assert(mid);
    info     = margo_get_info(handle);
    provider = margo_registered_data(mid, info->id);
    if (!provider) {
        out.ret = QTN_ERR_UNKNOWN_PROVIDER;
        goto finish;
    }

    /* note that this rpc doesn't have any input */
    memset(&out, 0, sizeof(out));
    out.cfg_str = margo_get_config(mid);

finish:
    margo_respond(handle, &out);
    if (out.cfg_str) free(out.cfg_str);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(qtn_get_server_config_ult)
