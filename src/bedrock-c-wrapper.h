#ifndef BEDROCK_C_WRAPPER
#define BEDROCK_C_WRAPPER

#include <margo.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BEDROCK_SUCCESS 0

typedef struct bedrock_client*  bedrock_client_t;
typedef struct bedrock_service* bedrock_service_t;

int bedrock_client_init(margo_instance_id mid, bedrock_client_t* client);

int bedrock_client_finalize(bedrock_client_t client);

int bedrock_service_handle_create(bedrock_client_t,
                                  const char*        address,
                                  uint16_t           provider_id,
                                  bedrock_service_t* sh);

int bedrock_service_handle_destroy(bedrock_service_t sh);

char* bedrock_service_query_config(bedrock_service_t sh, const char* script);

#ifdef __cplusplus
}
#endif

#endif
