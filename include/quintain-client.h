/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#ifndef __QUINTAIN_CLIENT_H
#define __QUINTAIN_CLIENT_H

#include <margo.h>
#include <quintain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QTN_CLIENT_NULL          ((quintain_client_t)NULL)
#define QTN_PROVIDER_HANDLE_NULL ((quintain_provider_handle_t)NULL)

typedef struct quintain_client*          quintain_client_t;
typedef struct quintain_provider_handle* quintain_provider_handle_t;

int quintain_client_init(margo_instance_id mid, quintain_client_t* client);

int quintain_client_finalize(quintain_client_t client);

int quintain_provider_handle_create(quintain_client_t           client,
                                    hg_addr_t                   addr,
                                    uint16_t                    provider_id,
                                    quintain_provider_handle_t* handle);

int quintain_provider_handle_release(quintain_provider_handle_t handle);

int quintain_work(quintain_provider_handle_t provider,
                  int                        req_buffer_size,
                  int                        resp_buffer_size,
                  hg_size_t                  bulk_size,
                  void*                      bulk_buffer,
                  int                        flags,
                  int                        op_flags);

int quintain_stat(quintain_provider_handle_t provider,
                  double*                    utime_sec,
                  double*                    stime_sec,
                  double*                    alltime_sec);

#ifdef __cplusplus
}
#endif

#endif /* __QUINTAIN_CLIENT_H */
