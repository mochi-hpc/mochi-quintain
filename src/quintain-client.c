/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include <string.h>
#include <stdlib.h>
#include <margo.h>
#include <quintain-client.h>

struct quintain_client {
    margo_instance_id mid;

    /* hg_id_t foo */

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
    quintain_client_t c = (quintain_client_t)calloc(1, sizeof(*c));
    if (!c) return QTN_ERR_ALLOCATION;

    c->num_provider_handles = 0;
    c->mid                  = mid;

    /* TODO: register RPCs */

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