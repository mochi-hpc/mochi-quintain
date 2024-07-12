#include "bedrock-c-wrapper.h"
#include <bedrock/Client.hpp>
#include <bedrock/ServiceHandle.hpp>

struct bedrock_client {
    bedrock::Client inner;
};

struct bedrock_service {
    bedrock::ServiceHandle inner;
};

extern "C" int bedrock_client_init(margo_instance_id mid,
                                   bedrock_client_t* client)
{
    *client = new bedrock_client{bedrock::Client{mid}};
    return BEDROCK_SUCCESS;
}

extern "C" int bedrock_client_finalize(bedrock_client_t client)
{
    delete client;
    return BEDROCK_SUCCESS;
}

extern "C" int bedrock_service_handle_create(bedrock_client_t   client,
                                             const char*        address,
                                             uint16_t           provider_id,
                                             bedrock_service_t* sh)
{
    *sh = new bedrock_service{
        client->inner.makeServiceHandle(address, provider_id)};
    return BEDROCK_SUCCESS;
}

extern "C" int bedrock_service_handle_destroy(bedrock_service_t sh)
{
    delete sh;
    return BEDROCK_SUCCESS;
}

extern "C" char* bedrock_service_query_config(bedrock_service_t sh,
                                              const char*       script)
{
    std::string config;
    sh->inner.queryConfig(script, &config);
    return strdup(config.c_str());
}
