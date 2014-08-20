/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef LIB_XOMP_H_
#define LIB_XOMP_H_

/// arguments passed to worker domains
#define XOMP_WORKER_ARG "--xompworker"

/* XOMP channel settings */

/// size of the messaging channel between master and remote workers
#define XOMP_MSG_CHAN_SIZE 4096

/// size of the thread local storage frame
#define XOMP_TLS_SIZE 4096

/// size of the messaging frame (2x channel size)
#define XOMP_MSG_FRAME_SIZE (2* XOMP_MSG_CHAN_SIZE)

/// total size of the frame to allocate
#define XOMP_FRAME_SIZE (XOMP_MSG_FRAME_SIZE + XOMP_TLS_SIZE)

/* Typedefs */

/// XOMP worker ID
typedef uint64_t xomp_wid_t;

/// XOMP task ID
typedef uint32_t xomp_task_id_t;

/// possible frame types when adding a new shared frame
typedef enum xomp_frame_type {
    XOMP_FRAME_TYPE_SHARED_RW,
    XOMP_FRAME_TYPE_SHARED_RO,
    XOMP_FRAME_TYPE_REPL_RW,
    XOMP_FRAME_TYPE_REPL_RO,
    XOMP_FRAME_TYPE_MSG
} xomp_frame_type_t;

/// controls where the worker are spawned
typedef enum xomp_worker_loc {
    XOMP_WORKER_LOC_INVALID = 0,
    XOMP_WORKER_LOC_MIXED,  ///< spawn remote and local
    XOMP_WORKER_LOC_REMOTE, ///< spawn remote only
    XOMP_WORKER_LOC_LOCAL   ///< spawn local only
} xomp_wloc_t;

typedef enum xomp_arg_type {
    XOMP_ARG_TYPE_WORKER,
    XOMP_ARG_TYPE_UNIFORM,
    XOMP_ARG_TYPE_DISTINCT,
} xomp_arg_t;

/// XOMP worker function
typedef void (*xomp_worker_fn_t) (void *);

/**
 * XOMP task structure
 */
struct xomp_task
{
    xomp_task_id_t id;          ///< id of the task
    uint32_t total_threads;     ///< total workers used in this task
    uint32_t done;              ///< number of workers that have completed
    void *barrier;              ///< the barrier to enter upon completion
    void *arg;                  ///< argument of the worker function
    xomp_worker_fn_t fn;        ///< worker function to be called
};

/**
 *
 */
struct xomp_spawn {
    uint8_t argc;
    char **argv;
    char *path;
};

/**
 *
 */
struct xomp_args
{
    xomp_arg_t type;
    union {
        struct {
            xomp_wid_t id;
        } worker;
        struct {
            uint32_t nthreads;
            xomp_wloc_t worker_loc;
            uint8_t nphi;
            coreid_t core_stride;
            uint8_t argc;
            char **argv;
        } uniform;
        struct {
            uint32_t nthreads;
            xomp_wloc_t worker_loc;
            uint8_t nphi;
            coreid_t core_stride;
            struct xomp_spawn local;
            struct xomp_spawn remote;
        } distinct;
    } args;
};

/* include the master and worker */
#include <xomp/xomp_master.h>
#include <xomp/xomp_worker.h>

#endif // LIB_XOMP_H_

