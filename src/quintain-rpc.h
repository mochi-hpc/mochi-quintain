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

typedef struct {
    uint64_t
        resp_buffer_size; /* size of buffer provider should give in response */
    uint64_t  req_buffer_size; /* size of buffer in this request */
    uint64_t  bulk_size;       /* bulk xfer size */
    uint32_t  flags;           /* flags to modify behavior */
    uint32_t  bulk_op;         /* what type of bulk xfer to do */
    hg_bulk_t bulk_handle;     /* bulk handle (if set) for bulk xfer */
    uint32_t  operation;       /* what type of work we will carry out */
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

    /* these components are general, regardless of hg_proc_op_t */
    hg_proc_uint64_t(proc, &in->resp_buffer_size);
    hg_proc_uint64_t(proc, &in->req_buffer_size);
    hg_proc_uint64_t(proc, &in->bulk_size);
    hg_proc_uint32_t(proc, &in->bulk_op);
    hg_proc_uint32_t(proc, &in->operation);
    hg_proc_hg_bulk_t(proc, &in->bulk_handle);

    /* The remainder of the request contains the req_buffer; differentiate
     * how we handle it depending on the hg_proc_op_t mode.
     *
     * NOTE: the req_buffer and req_buffer_size are used to synthetically
     * vary the request size to mimic different workloads.  We use raw
     * memory for this purpose with no encoding.
     */
    switch (hg_proc_get_op(proc)) {
    case HG_ENCODE:
        if (in->req_buffer_size) {
            /* get pointer to encoded buffer position and directly copy
             * data into it
             */
            buf = hg_proc_save_ptr(proc, in->req_buffer_size);
            memcpy(buf, in->req_buffer, in->req_buffer_size);
            hg_proc_restore_ptr(proc, buf, in->req_buffer_size);
        }
        break;
    case HG_DECODE:
        if (in->req_buffer_size) {
            /* set decoded pointer to reference appropriate offset
             * in encoded buffe
             */
            buf            = hg_proc_save_ptr(proc, in->req_buffer_size);
            in->req_buffer = buf;
            hg_proc_restore_ptr(proc, buf, in->req_buffer_size);
        }
        break;
    case HG_FREE:
        /* nothing to do here */
        break;
    }

    return (HG_SUCCESS);
}

static inline hg_return_t hg_proc_qtn_work_out_t(hg_proc_t proc, void* v_out_p)
{
    qtn_work_out_t* out = v_out_p;
    void*           buf = NULL;

    /* these components are general, regardless of hg_proc_op_t */
    hg_proc_uint32_t(proc, &out->ret);
    hg_proc_uint64_t(proc, &out->resp_buffer_size);

    /* The remainder of the response contains the resp_buffer; differentiate
     * how we handle it depending on the hg_proc_op_t mode.
     *
     * NOTE: the resp_buffer and resp_buffer_size are used to synthetically
     * vary the response size to mimic different workloads.  We use raw
     * memory for this purpose with no encoding.
     */
    switch (hg_proc_get_op(proc)) {
    case HG_ENCODE:
        if (out->resp_buffer_size) {
            /* get pointer to encoded buffer position and directly copy
             * data into it
             */
            buf = hg_proc_save_ptr(proc, out->resp_buffer_size);
            memcpy(buf, out->resp_buffer, out->resp_buffer_size);
            hg_proc_restore_ptr(proc, buf, out->resp_buffer_size);
        }
        break;
    case HG_DECODE:
        if (out->resp_buffer_size) {
            /* set decoded pointer to reference appropriate offset
             * in encoded buffe
             */
            buf              = hg_proc_save_ptr(proc, out->resp_buffer_size);
            out->resp_buffer = buf;
            hg_proc_restore_ptr(proc, buf, out->resp_buffer_size);
        }
        break;
    case HG_FREE:
        /* nothing to do here */
        break;
    }

    return (HG_SUCCESS);
}

MERCURY_GEN_PROC(qtn_stat_out_t,
                 ((int32_t)(ret))((int64_t)(utime_sec))((int64_t)(utime_usec))(
                     (int64_t)(stime_sec))((int64_t)(stime_usec)))

#endif /* __QUINTAIN_RPC */
