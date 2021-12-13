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

struct quintain_provider {
    margo_instance_id mid;
    ABT_pool handler_pool; // pool used to run RPC handlers for this provider

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

    /* check if a provider with the same provider id already exists */
    {
        hg_id_t   id;
        hg_bool_t flag;
        margo_provider_registered_name(mid, "quintain_probe_rpc", provider_id,
                                       &id, &flag);
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
