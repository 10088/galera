/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */
/*
 * This header defines generic communication layer
 * which implements basic open/close/send/receive
 * functions. Its purpose is to implement all
 * functionality common to all group communication
 * uses. Currently this amounts to action
 * fragmentation/defragmentation and invoking backend
 * functions.
 * In the course of development it has become clear
 * that such fuctionality must be collected in a
 * separate layer.
 * Application abstraction layer is based on this one
 * and uses those functions for its own purposes.
 */

#ifndef _gcs_core_h_
#define _gcs_core_h_

#include <stdint.h>
#include <stdlib.h>

#include <galerautils.h>

#include "gcs.h"

struct gcs_core;
typedef struct gcs_core gcs_core_t; 

/*
 * Allocates context resources  private to
 * generic communicaton layer - send/recieve buffers and the like.
 */
extern gcs_core_t*
gcs_core_create (const char* node_name,
                 const char* inc_addr);

/* initializes action history (global seqno, group UUID). See gcs.h */
extern long
gcs_core_init (gcs_core_t* core, gcs_seqno_t seqno, const gu_uuid_t* uuid);

/*
 * gcs_core_open() opens connection
 * Return values:
 * zero     - success
 * negative - error code
 */
extern long
gcs_core_open  (gcs_core_t* conn,
                const char* channel,
                const char* url);


/*
 * gcs_core_close() puts connection in a closed state,
 * cancelling all ongoing calls.
 * Return values:
 * zero     - success
 * negative - error code
 */
extern long
gcs_core_close (gcs_core_t* conn);

/*
 * gcs_core_destroy() frees resources allocated by gcs_core_create()
 * Return values:
 * zero     - success
 * negative - error code
 */
extern long
gcs_core_destroy (gcs_core_t* conn);

/* 
 * gcs_core_send() atomically sends action to group.
 *
 * NOT THREAD SAFE! Access should be serialized.
 *
 * Return values:
 * non-negative - amount of action bytes sent (sans headers)
 * negative     - error code
 *                -EAGAIN   - operation should be retried
 *                -ENOTCONN - connection to primary component lost
 *
 * NOTE: Successful return code here does not guarantee delivery to group.
 *       The real status of action is determined only in gcs_core_recv() call.
 */
extern ssize_t
gcs_core_send (gcs_core_t*     conn,
               const void*     action,
               size_t          act_size,
               gcs_act_type_t  act_type);

/*
 * gcs_core_recv() blocks until some action is received from group.
 * Return values:
 * non-negative - the size of action received
 * negative     - error code
 *
 * NOTE: Action status (replicated or not) is carried in act_id. E.g. -ENOTCONN
 *       means connection to primary component was lost while sending,
 *       -ERESTART means that action delivery was interrupted and it must be
 *       resent.
 */
extern ssize_t
gcs_core_recv (gcs_core_t*     conn,
               const void**    action,
               gcs_act_type_t* act_type,
               gcs_seqno_t*    act_id);

/* Configuration functions */
/* Sets maximum message size to achieve requested network packet size. 
 * In case of failure returns negative error code, in case of success -
 * resulting message payload size (size of action fragment) */
extern long
gcs_core_set_pkt_size (gcs_core_t* conn, ulong pkt_size);

/* sends this node's last applied value to group */
extern long
gcs_core_set_last_applied (gcs_core_t* core, gcs_seqno_t seqno);

/* sends status of the ended snapshot (snapshot seqno or error code) */
extern long
gcs_core_send_join (gcs_core_t* core, gcs_seqno_t seqno);

/* sends SYNC notice, seqno currently has no meaning */
extern long
gcs_core_send_sync (gcs_core_t* core, gcs_seqno_t seqno);

/* sends flow control message */
extern long
gcs_core_send_fc (gcs_core_t* core, const void* fc, size_t fc_size);

#ifdef GCS_CORE_TESTING

/* gcs_core_send() interface does not allow enough concurrency control to model
 * various race conditions for unit testing - it is not atomic. The functions
 * below expose gcs_core unit internals solely for the purpose of testing */

#include "gcs_msg_type.h"
#include "gcs_backend.h"

extern gcs_backend_t*
gcs_core_get_backend (gcs_core_t* core);

// switches lock-step mode on/off
extern void
gcs_core_send_lock_step (gcs_core_t* core, bool enable);

// step through action send process (send another fragment).
// returns positive number if there was a send thread waiting for it.
extern long
gcs_core_send_step (gcs_core_t* core, long timeout_ms);

#endif /* GCS_CORE_TESTING */

#endif /* _gcs_core_h_ */
