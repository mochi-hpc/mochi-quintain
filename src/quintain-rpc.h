/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#ifndef __QUINTAIN_RPC
#define __QUINTAIN_RPC

#include <margo.h>
#include <mercury_proc_string.h>
#include <quintain.h>

MERCURY_GEN_PROC(qtn_get_server_config_out_t,
                 ((int32_t)(ret))((hg_string_t)(cfg_str)))

#endif /* __QUINTAIN_RPC */
