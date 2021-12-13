/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include <string.h>
#include <bedrock/module.h>
#include <quintain-server.h>
#include <quintain-client.h>

static int quintain_register_provider(bedrock_args_t             args,
                                      bedrock_module_provider_t* provider)
{
    int                                ret;
    struct quintain_provider_init_info bpargs = {0};
    margo_instance_id mid         = bedrock_args_get_margo_instance(args);
    uint16_t          provider_id = bedrock_args_get_provider_id(args);
    ABT_pool          pool        = bedrock_args_get_pool(args);
    const char*       config      = bedrock_args_get_config(args);
    const char*       name        = bedrock_args_get_name(args);

    QTN_TRACE(mid, "quintain_register_provider()");
    QTN_TRACE(mid, " -> mid           = %p", (void*)mid);
    QTN_TRACE(mid, " -> provider id   = %d", provider_id);
    QTN_TRACE(mid, " -> pool          = %p", (void*)pool);
    QTN_TRACE(mid, " -> config        = %s", config);
    QTN_TRACE(mid, " -> name          = %s", name);

    bpargs.json_config = config;

    ret = quintain_provider_register(mid, provider_id, &bpargs,
                                     (quintain_provider_t*)provider);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

static int quintain_deregister_provider(bedrock_module_provider_t provider)
{
    int ret;

    ret = quintain_provider_deregister(provider);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

static char* quintain_get_provider_config(bedrock_module_provider_t provider)
{
#if 0
    return (quintain_provider_get_config(provider));
#else
    return strdup("{}");
#endif
}

static int quintain_init_client(bedrock_args_t           args,
                                bedrock_module_client_t* client)
{
    int ret;

    margo_instance_id mid = bedrock_args_get_margo_instance(args);
    QTN_TRACE(mid, "quintain_init_client()");

    ret = quintain_client_init(mid, (quintain_client_t*)client);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

static int quintain_finalize_client(bedrock_module_client_t client)
{
    int ret;

    ret = quintain_client_finalize(client);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

static char* quintain_get_client_config(bedrock_module_client_t client)
{
    return strdup("{}");
}

static int quintain_create_provider_handle(bedrock_module_client_t client,
                                           hg_addr_t               address,
                                           uint16_t                provider_id,
                                           bedrock_module_provider_handle_t* ph)
{
    int ret;

    ret = quintain_provider_handle_create(client, address, provider_id,
                                          (quintain_provider_handle_t*)ph);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

static int quintain_destroy_provider_handle(bedrock_module_provider_handle_t ph)
{
    int ret;

    ret = quintain_provider_handle_release(ph);
    if (ret < 0) return (-1);

    return BEDROCK_SUCCESS;
}

struct bedrock_dependency quintain_provider_deps[1]
    = {BEDROCK_NO_MORE_DEPENDENCIES};

struct bedrock_dependency quintain_client_deps[1]
    = {BEDROCK_NO_MORE_DEPENDENCIES};

static struct bedrock_module quintain
    = {.register_provider       = quintain_register_provider,
       .deregister_provider     = quintain_deregister_provider,
       .get_provider_config     = quintain_get_provider_config,
       .init_client             = quintain_init_client,
       .finalize_client         = quintain_finalize_client,
       .get_client_config       = quintain_get_client_config,
       .create_provider_handle  = quintain_create_provider_handle,
       .destroy_provider_handle = quintain_destroy_provider_handle,
       .provider_dependencies   = quintain_provider_deps,
       .client_dependencies     = quintain_client_deps};

BEDROCK_REGISTER_MODULE(quintain, quintain)
