/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include <string.h>
#include <stdlib.h>
#include <margo.h>
#include <quintain-client.h>

#include "quintain-rpc.h"

struct quintain_client {
    margo_instance_id mid;

    hg_id_t qtn_work_rpc_id;

    uint64_t num_provider_handles;
};

struct quintain_provider_handle {
    struct quintain_client* client;
    hg_addr_t               addr;
    uint16_t                provider_id;
    uint64_t                refcount;
};

int quintain_client_init(margo_instance_id mid, quintain_client_t* client)
{
    hg_bool_t already_registered_flag;
    hg_id_t   id;

    quintain_client_t c = (quintain_client_t)calloc(1, sizeof(*c));
    if (!c) return QTN_ERR_ALLOCATION;

    c->num_provider_handles = 0;
    c->mid                  = mid;

    margo_registered_name(mid, "qtn_work_rpc", &id, &already_registered_flag);

    if (already_registered_flag == HG_TRUE) { /* RPCs already registered */
        margo_registered_name(mid, "qtn_work_rpc", &c->qtn_work_rpc_id,
                              &already_registered_flag);
    } else { /* RPCs not already registered */
        c->qtn_work_rpc_id = MARGO_REGISTER(mid, "qtn_work_rpc", qtn_work_in_t,
                                            qtn_work_out_t, NULL);
    }

    *client = c;
    return QTN_SUCCESS;
}

int quintain_client_finalize(quintain_client_t client)
{
    if (client->num_provider_handles != 0) {
        QTN_WARNING(client->mid,
                    "%llu provider handles not released before "
                    "quintain_client_finalize was called\n",
                    (long long unsigned int)client->num_provider_handles);
    }
    free(client);
    return QTN_SUCCESS;
}

int quintain_provider_handle_create(quintain_client_t           client,
                                    hg_addr_t                   addr,
                                    uint16_t                    provider_id,
                                    quintain_provider_handle_t* handle)
{
    if (client == QTN_CLIENT_NULL) return QTN_ERR_INVALID_ARG;

    quintain_provider_handle_t provider
        = (quintain_provider_handle_t)calloc(1, sizeof(*provider));

    if (!provider) return QTN_ERR_ALLOCATION;

    hg_return_t ret = margo_addr_dup(client->mid, addr, &(provider->addr));
    if (ret != HG_SUCCESS) {
        free(provider);
        return QTN_ERR_MERCURY;
    }

    provider->client      = client;
    provider->provider_id = provider_id;
    provider->refcount    = 1;

    client->num_provider_handles += 1;

    *handle = provider;
    return QTN_SUCCESS;
}

int quintain_provider_handle_release(quintain_provider_handle_t handle)
{
    if (handle == QTN_PROVIDER_HANDLE_NULL) return QTN_ERR_INVALID_ARG;
    handle->refcount -= 1;
    if (handle->refcount == 0) {
        margo_addr_free(handle->client->mid, handle->addr);
        handle->client->num_provider_handles -= 1;
        free(handle);
    }
    return QTN_SUCCESS;
}

int quintain_work(quintain_provider_handle_t provider,
                  int                        req_buffer_size,
                  int                        resp_buffer_size,
                  hg_size_t                  bulk_size,
                  hg_bulk_op_t               bulk_op,
                  void*                      bulk_buffer,
                  int                        flags)
{
    hg_handle_t    handle = HG_HANDLE_NULL;
    qtn_work_in_t  in;
    qtn_work_out_t out;
    int            ret = 0;
    hg_return_t    hret;
    int            bulk_flags = HG_BULK_READ_ONLY;

    hret = margo_create(provider->client->mid, provider->addr,
                        provider->client->qtn_work_rpc_id, &handle);
    if (hret != HG_SUCCESS) {
        ret = QTN_ERR_MERCURY;
        goto finish;
    }

    in.bulk_op = bulk_op;
    if (bulk_op == HG_BULK_PUSH) bulk_flags = HG_BULK_WRITE_ONLY;
    in.bulk_handle      = HG_BULK_NULL;
    in.resp_buffer_size = resp_buffer_size;
    in.req_buffer_size  = req_buffer_size;
    if (req_buffer_size)
        in.req_buffer = calloc(1, req_buffer_size);
    else
        in.req_buffer = NULL;
    in.bulk_size = bulk_size;
    if (bulk_size) {
        hret = margo_bulk_create(provider->client->mid, 1,
                                 (void**)(&bulk_buffer), &bulk_size, bulk_flags,
                                 &in.bulk_handle);
        if (hret != HG_SUCCESS) {
            ret = QTN_ERR_MERCURY;
            QTN_ERROR(provider->client->mid, "margo_bulk_create: %s",
                      HG_Error_to_string(hret));
            goto finish;
        }
    }

    hret = margo_provider_forward(provider->provider_id, handle, &in);
    if (hret != HG_SUCCESS) {
        ret = QTN_ERR_MERCURY;
        QTN_ERROR(provider->client->mid, "margo_provider_forward: %s",
                  HG_Error_to_string(hret));
        goto finish;
    }

    hret = margo_get_output(handle, &out);
    if (hret != HG_SUCCESS) {
        ret = QTN_ERR_MERCURY;
        QTN_ERROR(provider->client->mid, "margo_get_output: %s",
                  HG_Error_to_string(hret));
        goto finish;
    }

    ret = out.ret;

finish:

    if (in.bulk_handle != HG_BULK_NULL) margo_bulk_free(in.bulk_handle);
    if (in.req_buffer) free(in.req_buffer);
    if (hret == HG_SUCCESS) margo_free_output(handle, &out);
    if (handle != HG_HANDLE_NULL) margo_destroy(handle);

    return (ret);
}
