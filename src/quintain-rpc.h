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

typedef struct {
    uint64_t
        resp_buffer_size; /* size of buffer provider should give in response */
    uint64_t  req_buffer_size; /* size of buffer in this request */
    uint64_t  bulk_size;       /* bulk xfer size */
    uint32_t  bulk_op;         /* what type of bulk xfer to do */
    hg_bulk_t bulk_handle;     /* bulk handle (if set) for bulk xfer */
    char*     req_buffer;      /* dummy buffer */
} qtn_work_in_t;
static inline hg_return_t hg_proc_qtn_work_in_t(hg_proc_t proc, void* v_out_p);

typedef struct {
    uint64_t resp_buffer_size; /* size of buffer in this response */
    char*    resp_buffer;      /* dummy buffer */
    int32_t  ret;              /* return code */
} qtn_work_out_t;
static inline hg_return_t hg_proc_qtn_work_out_t(hg_proc_t proc, void* v_out_p);

static inline hg_return_t hg_proc_qtn_work_in_t(hg_proc_t proc, void* v_out_p)
{
    qtn_work_in_t* in  = v_out_p;
    void*          buf = NULL;

    hg_proc_uint64_t(proc, &in->resp_buffer_size);
    hg_proc_uint64_t(proc, &in->req_buffer_size);
    hg_proc_uint64_t(proc, &in->bulk_size);
    hg_proc_uint32_t(proc, &in->bulk_op);
    hg_proc_hg_bulk_t(proc, &in->bulk_handle);
    /* pack dummy data in request if requested */
    if (in->req_buffer_size) {
        buf = hg_proc_save_ptr(proc, in->req_buffer_size);
        if (hg_proc_get_op(proc) == HG_ENCODE)
            memcpy(buf, in->req_buffer, in->req_buffer_size);
        if (hg_proc_get_op(proc) == HG_DECODE) in->req_buffer = buf;
        hg_proc_restore_ptr(proc, buf, in->req_buffer_size);
    }

    return (HG_SUCCESS);
}

static inline hg_return_t hg_proc_qtn_work_out_t(hg_proc_t proc, void* v_out_p)
{
    qtn_work_out_t* out = v_out_p;
    void*           buf = NULL;

    hg_proc_uint64_t(proc, &out->resp_buffer_size);
    if (out->resp_buffer_size) {
        buf = hg_proc_save_ptr(proc, out->resp_buffer_size);
        if (hg_proc_get_op(proc) == HG_ENCODE)
            memcpy(buf, out->resp_buffer, out->resp_buffer_size);
        if (hg_proc_get_op(proc) == HG_DECODE) out->resp_buffer = buf;
        hg_proc_restore_ptr(proc, buf, out->resp_buffer_size);
    }
    hg_proc_uint32_t(proc, &out->ret);

    return (HG_SUCCESS);
}

#endif /* __QUINTAIN_RPC */
