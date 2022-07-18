/*
 * (C) 2021 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#ifndef __QUINTAIN_H
#define __QUINTAIN_H

#include <margo.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QTN_TRACE(_mid, _format, f...) \
    margo_trace(_mid, "quintain: " _format, ##f)
#define QTN_DEBUG(_mid, _format, f...) \
    margo_debug(_mid, "quintain: " _format, ##f)
#define QTN_INFO(_mid, _format, f...) \
    margo_info(_mid, "quintain: " _format, ##f)
#define QTN_WARNING(_mid, _format, f...) \
    margo_warning(_mid, "quintain: " _format, ##f)
#define QTN_ERROR(_mid, _format, f...) \
    margo_error(_mid, "quintain: " _format, ##f)
#define QTN_CRITICAL(_mid, _format, f...) \
    margo_critical(_mid, "quintain: " _format, ##f)

#define QTN_SUCCESS              0    /* success */
#define QTN_ERR_ALLOCATION       (-1) /* error allocating something */
#define QTN_ERR_INVALID_ARG      (-2) /* invalid argument */
#define QTN_ERR_MERCURY          (-3) /* Mercury error */
#define QTN_ERR_UNKNOWN_PROVIDER (-4) /* can't find provider */

/* flags for workload operations */
enum {
    QTN_WORK_USE_SERVER_POOLSET,
    QTN_WORK_CACHE_UPDATE,
    QTN_WORK_CACHE_WRITE,
    QTN_WORK_CACHE_READ
};

#ifdef __cplusplus
}
#endif

#endif /* __QUINTAIN_H */
