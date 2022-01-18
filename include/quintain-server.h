/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#ifndef __QUINTAIN_SERVER_H
#define __QUINTAIN_SERVER_H

#include <margo.h>
#include <quintain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QTN_PROVIDER_ID_DEFAULT 0
#define QTN_PROVIDER_IGNORE     NULL

typedef struct quintain_provider* quintain_provider_t;

/**
 * The quintain_provider_init_info structure can be passed in to the
 * quintain_provider_register() function to configure the provider. The struct
 * can be memset to zero to use default values.
 */
struct quintain_provider_init_info {
    const char* json_config; /* optional JSON-formatted string */
    ABT_pool    rpc_pool;    /* optional pool on which to run RPC handlers */
};

/**
 * Example JSON configuration:
 * ----------------------------------------------
{
}
*/

#define QTN_PROVIDER_INIT_INFO_INITIALIZER \
    {                                      \
        NULL, ABT_POOL_NULL                \
    }

/**
 * Initializes a QTN provider.
 *
 * @param[in] mid Margo instance identifier
 * @param[in] provider_id provider id
 * @param[out] provider resulting provider
 * @returns 0 on success, -1 otherwise
 */
int quintain_provider_register(margo_instance_id mid,
                               uint16_t          provider_id,
                               const struct quintain_provider_init_info* args,
                               quintain_provider_t* provider);

/**
 * @brief Deregisters the provider.
 *
 * @param provider Provider to deregister.
 *
 * @return 0 on success, -1 otherwise.
 */
int quintain_provider_deregister(quintain_provider_t provider);

/**
 * Retrieves complete configuration of quintain provider, encoded as json
 *
 * @param [in] provider quintain provider
 * @returns null terminated string that must be free'd by caller
 */
char* quintain_provider_get_config(quintain_provider_t provider);

#ifdef __cplusplus
}
#endif

#endif /* __QUINTAIN_SERVER_H */
