/*
 *  $Id: flowcontrol.c,v 1.1 2003/05/16 13:47:49 ajung Exp $
 * SCTP implementation according to RFC 2960.
 * Copyright (C) 2000 by Siemens AG, Munich, Germany.
 *
 * Realized in co-operation between Siemens AG
 * and University of Essen, Institute of Computer Networking Technology.
 *
 * Acknowledgement
 * This work was partially funded by the Bundesministerium für Bildung und
 * Forschung (BMBF) of the Federal Republic of Germany (Förderkennzeichen 01AK045).
 * The authors alone are responsible for the contents.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * There are two mailinglists available at http://www.sctp.de which should be
 * used for any discussion related to this implementation.
 *
 * Contact: discussion@sctp.de
 *          Michael.Tuexen@icn.siemens.de
 *          ajung@exp-math.uni-essen.de
 *
 * This module implements most parts of the flow control mechanisms
 *
 */

#include "flowcontrol.h"
#include "bundling.h"
#include "adaptation.h"
#include "recvctrl.h"

#include <stdio.h>
#include <glib.h>

/* here we should not have to worry about wrap */
#define max(x,y)            ((x)>(y))?(x):(y)
#define min(x,y)            ((x)<(y))?(x):(y)

/* #define Current_event_log_ 6 */
/**
 * this struct contains all relevant congestion control parameters for
 * one PATH to the destination/association peer endpoint
 */
typedef struct __congestion_parameters
{
    //@{
    ///
    unsigned int cwnd;
    ///
    unsigned int cwnd2;
    ///
    unsigned int partial_bytes_acked;
    ///
    unsigned int ssthresh;
    ///
    unsigned int outstanding_bytes_per_address;
    ///
    unsigned int mtu;
    ///
    struct timeval time_of_cwnd_adjustment;
    //@}
} cparm;

typedef struct flowcontrol_struct
{
    //@{
    ///
    unsigned int outstanding_bytes;
    ///
    unsigned int announced_rwnd;
    ///
    unsigned int number_of_addresses;
    /** pointer to array of congestion window parameters */
    cparm *cparams;
    ///
    unsigned int current_tsn;
    ///
    GList *chunk_list;
    ///
    unsigned int list_length;
    ///
    unsigned int last_active_address;
    ///
    TimerID cwnd_timer;
    /** one timer may be running per destination address */
    TimerID *T3_timer;
    /** for passing as parameter in callback functions */
    unsigned int *addresses;
    ///
    unsigned int my_association;
    ///
    boolean shutdown_received;
    ///
    boolean waiting_for_sack;
    ///
    boolean t3_retransmission_sent;
    ///
    boolean one_packet_inflight;
    ///
    unsigned int maxQueueLen;
    //@}
} fc_data;


/* ---------------  Function Prototypes -----------------------------*/
int fc_check_for_txmit(void *fc_instance, unsigned int oldListLen);
/* ---------------  Function Prototypes -----------------------------*/


/**
 * Creates new instance of flowcontrol module and returns pointer to it
 * TODO : should parameter be unsigned short ?
 * TODO : get and update MTU (guessed values ?) per destination address
 * @param  peer_rwnd receiver window that peer allowed us when setting up the association
 * @param  my_iTSN my initial TSN value
 * @param  number_of_destination_addresses the number of paths to the association peer
 * @return  pointer to the new fc_data instance
*/
void *fc_new_flowcontrol(unsigned int peer_rwnd,
                         unsigned int my_iTSN,
                         unsigned int number_of_destination_addresses,
                         unsigned int maxQueueLen)
{
    fc_data *tmp;
    unsigned int count;

    tmp = malloc(sizeof(fc_data));
    if (!tmp)
        error_log(ERROR_FATAL, "Malloc failed");
    tmp->current_tsn = my_iTSN;

    event_logi(VERBOSE,
               "Flowcontrol: ===== Num of number_of_destination_addresses = %d ",
               number_of_destination_addresses);

    tmp->cparams = malloc(number_of_destination_addresses * sizeof(cparm));
    if (!tmp->cparams)
        error_log(ERROR_FATAL, "Malloc failed");

    tmp->T3_timer = malloc(number_of_destination_addresses * sizeof(TimerID));
    if (!tmp->T3_timer)
        error_log(ERROR_FATAL, "Malloc failed");

    tmp->addresses = malloc(number_of_destination_addresses * sizeof(unsigned int));
    if (!tmp->addresses)
        error_log(ERROR_FATAL, "Malloc failed");

    for (count = 0; count < number_of_destination_addresses; count++) {
        tmp->T3_timer[count] = 0; /* i.e. timer not running */
        tmp->addresses[count] = count;
        (tmp->cparams[count]).cwnd = 2 * MAX_MTU_SIZE;
        (tmp->cparams[count]).cwnd2 = 0L;
        (tmp->cparams[count]).partial_bytes_acked = 0L;
        (tmp->cparams[count]).ssthresh = peer_rwnd;
        (tmp->cparams[count]).outstanding_bytes_per_address = 0L;
        (tmp->cparams[count]).mtu = MAX_SCTP_PDU;
        adl_gettime( &(tmp->cparams[count].time_of_cwnd_adjustment) );
    }
    tmp->outstanding_bytes = 0;
    tmp->announced_rwnd = peer_rwnd;
    tmp->cwnd_timer = 0;
    tmp->last_active_address = pm_readPrimaryPath();
    tmp->number_of_addresses = number_of_destination_addresses;
    tmp->waiting_for_sack = FALSE;
    tmp->shutdown_received = FALSE;
    tmp->t3_retransmission_sent = FALSE;
    tmp->one_packet_inflight = FALSE;
    tmp->chunk_list = NULL;
    tmp->maxQueueLen = maxQueueLen;
    tmp->list_length = 0;

    rtx_set_remote_receiver_window(peer_rwnd);

    tmp->my_association = mdi_readAssociationID();
    event_logi(VVERBOSE, "FlowControl : Association-ID== %d \n", tmp->my_association);
    if (tmp->my_association == 0)
        error_log(ERROR_FATAL, "Association was not set, should be......");
    return tmp;
}

/**
 * this function stops all currently running timers, and may be called when
 * the shutdown is imminent
 * @param  new_rwnd new receiver window of the association peer
 */
void fc_restart(guint32 new_rwnd, unsigned int iTSN, unsigned int maxQueueLen)
{
    fc_data *tmp;
    guint32 count;

    tmp = (fc_data *) mdi_readFlowControl();
    event_log(INTERNAL_EVENT_0, "fc_restart()... ");
    if (!tmp) {
        error_log(ERROR_MINOR, "fc_data instance not set !");
        return;
    }
    fc_stop_timers();
    for (count = 0; count < tmp->number_of_addresses; count++) {
        (tmp->cparams[count]).cwnd = 2 * MAX_MTU_SIZE;
        (tmp->cparams[count]).cwnd2 = 0L;
        (tmp->cparams[count]).partial_bytes_acked = 0L;
        (tmp->cparams[count]).ssthresh = new_rwnd;
        (tmp->cparams[count]).outstanding_bytes_per_address = 0L;
        (tmp->cparams[count]).mtu = MAX_SCTP_PDU;
        adl_gettime( &(tmp->cparams[count].time_of_cwnd_adjustment) );
    }
    tmp->outstanding_bytes = 0;
    tmp->announced_rwnd = new_rwnd;
    tmp->waiting_for_sack = FALSE;
    tmp->shutdown_received = FALSE;
    tmp->t3_retransmission_sent = FALSE;
    tmp->one_packet_inflight = FALSE;
    tmp->current_tsn = iTSN;
    tmp->maxQueueLen = maxQueueLen;
    rtx_set_remote_receiver_window(new_rwnd);
    if ((tmp->chunk_list) != NULL) {
        /* TODO : pass chunks in this list back up to the ULP ! */
        g_list_foreach(tmp->chunk_list, &free_list_element, GINT_TO_POINTER(1));
        error_log(ERROR_MINOR, "FLOWCONTROL RESTART : List is deleted...");
    }
    g_list_free(tmp->chunk_list);
    tmp->chunk_list = NULL;
    tmp->list_length = 0;
}

/**
 * Deletes data occupied by a flow_control data structure
 * @param fc_instance pointer to the flow_control data structure
 */
void fc_delete_flowcontrol(void *fc_instance)
{
    fc_data *tmp;

    tmp = (fc_data *) fc_instance;
    event_log(INTERNAL_EVENT_0, "fc_delete_flowcontrol(): stop timers and delete flowcontrol data");
    fc_stop_timers();
    free(tmp->cparams);
    free(tmp->T3_timer);
    free(tmp->addresses);
    if ((tmp->chunk_list) != NULL) {
        error_log(ERROR_MINOR, "FLOWCONTROL : List is deleted with chunks still queued...");
        g_list_foreach(tmp->chunk_list, &free_list_element, GINT_TO_POINTER(1));
    }
    g_list_free(tmp->chunk_list);
    tmp->chunk_list = NULL;
    free(fc_instance);
}

/**
 * function to print debug data (flow control parameters of all paths)
 *  @param event_log_level  INTERNAL_EVENT_0 INTERNAL_EVENT_1 EXTERNAL_EVENT_X EXTERNAL_EVENT
 */
void fc_debug_cparams(short event_log_level)
{
    fc_data *fc;
    unsigned int count;

    if (event_log_level <= Current_event_log_) {
        fc = (fc_data *) mdi_readFlowControl();
        if (!fc) {
            error_log(ERROR_MAJOR, "fc_data instance not set !");
            return;
        }
        event_log(event_log_level,
                  "----------------------------------------------------------------------");
        event_log(event_log_level, "Debug-output for Congestion Control Parameters ! ");
        event_logii(event_log_level,
                    "outstanding_bytes == %u; current_tsn == %u; ",
                    fc->outstanding_bytes, fc->current_tsn);
        event_logii(event_log_level,
                    "chunks queued in flowcontrol== %lu; last_active_address == %u ",
                    fc->list_length, fc->last_active_address);
        event_logii(event_log_level,
                    "shutdown_received == %s; waiting_for_sack == %s",
                    ((fc->shutdown_received == TRUE) ? "TRUE" : "FALSE"),
                    ((fc->waiting_for_sack == TRUE) ? "TRUE" : "FALSE"));

        event_logi(event_log_level, "t3_retransmission_sent == %s ",
                   ((fc->t3_retransmission_sent == TRUE) ? "TRUE" : "FALSE"));
        for (count = 0; count < fc->number_of_addresses; count++) {
            event_logiiii(event_log_level,
                          "%u   %u   %u  address=%u XYZ",
                          (fc->cparams[count]).outstanding_bytes_per_address,
                          (fc->cparams[count]).cwnd, (fc->cparams[count]).ssthresh,count);
            event_logiiiii(event_log_level,
                           "%u :  mtu=%u   T3=%u   cwnd2=%u   pb_acked=%u",
                           count, (fc->cparams[count]).mtu,
                           fc->T3_timer[count], (fc->cparams[count]).cwnd2,
                           (fc->cparams[count]).partial_bytes_acked);
        }
        event_log(event_log_level,
                  "----------------------------------------------------------------------");

    }
    return;
}


/**
 * this function should be called to signal to flowcontrol, that our ULP
 * has initiated a shutdown procedure. We must only send unacked data from
 * now on ! The association is about to terminate !
 */
void fc_shutdown()
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();
    event_log(VERBOSE, "fc_shutdown()... ");
    if (!fc) {
        error_log(ERROR_MINOR, "fc_data instance not set !");
        return;
    }
    fc->shutdown_received = TRUE;
    return;
}

int fc_decrease_outstanding_bytes(unsigned int address, unsigned int numOfBytes)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();
    event_log(VERBOSE, "fc_adjust_outstanding_bytes()");
    if (!fc) {
        error_log(ERROR_MINOR, "fc_data instance not set !");
        return -1;
    }
    if (fc->cparams[address].outstanding_bytes_per_address >= numOfBytes) {
        fc->cparams[address].outstanding_bytes_per_address -= numOfBytes;
    } else {
        fc->cparams[address].outstanding_bytes_per_address = 0;
    }

    if (fc->outstanding_bytes >= numOfBytes) {
        fc->outstanding_bytes -= numOfBytes;
    } else {
        fc->outstanding_bytes = 0;
    }
    return 0;
}


/**
 * this function stops all currently running timers of the flowcontrol module
 * and may be called when the shutdown is imminent
 */
void fc_stop_timers(void)
{
    fc_data *fc;
    int count, result;

    fc = (fc_data *) mdi_readFlowControl();
    event_log(INTERNAL_EVENT_0, "fc_stop_timers()... ");
    if (!fc) {
        error_log(ERROR_MINOR, "fc_data instance not set !");
        return;
    }
    for (count = 0; count < fc->number_of_addresses; count++) {
        if (fc->T3_timer[count] != 0) {
            result = sctp_stopTimer(fc->T3_timer[count]);
            fc->T3_timer[count] = 0;
            if (result == 1)
                error_log(ERROR_MINOR, "Timer not correctly reset to 0 !");
            event_logii(VVERBOSE, "Stopping T3-Timer(%d) = %d ", count, result);
        }
    }
    if (fc->cwnd_timer != 0) {
        result = sctp_stopTimer(fc->cwnd_timer);
        fc->cwnd_timer = 0;
        if (result == 1)
            error_log(ERROR_MINOR, "Timer not correctly reset to 0 !");
        event_logi(VVERBOSE, "Stopping cwnd_timer = %d ", result);
    }

}


/**
 *  timer controlled callback function, that reduces cwnd, if data is not sent within a certain time.
 *  As all timer callbacks, it takes three arguments, the timerID, and two pointers to relevant data
 *  @param  tid the id of the timer that has gone off
 *  @param  assoc  pointer to the association structure, where cwnd needs to be reduced
 *  @param  data2  currently unused == NULL
 */
void fc_timer_cb_reduce_cwnd(TimerID tid, void *assoc, void *data2)
{
    /* TODO : verify that last_active_address is always set correctly */
    unsigned int ad_idx;
    fc_data *fc;
    int res;
    unsigned int rto;

    event_log(INTERNAL_EVENT_0, "----------------> fc_timer_cb_reduce_cwnd <------------------");
    event_logi(VVERBOSE, "Association ID == %d \n", *(unsigned int *) assoc);
    res = mdi_setAssociationData(*(unsigned int *) assoc);
    if (res == 1) {
        error_log(ERROR_MAJOR, " association does not exist !");
        return;
    }
    if (res == 2) {
        error_log(ERROR_MAJOR, "Association was not cleared..... !!!");
        /* failure treatment ? */
    }

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return;
    }

    /* try opening up, if possible */
    if (fc->outstanding_bytes == 0) {
        fc->one_packet_inflight = FALSE;
    }

    fc->cwnd_timer = 0L;
    /* function does nothing and effectively stops this timer if list is not empty */
    ad_idx = fc->last_active_address;
    if ((fc->chunk_list) == NULL) {
        rto = pm_readRTO(ad_idx);
        event_logii(INTERNAL_EVENT_0, "RTO for address %d was : %d msecs\n", ad_idx, rto);
        if (rto < 2)
            rto = 2;
        /* we have nothing to send, so cut cwnd in half ! */
        fc->cparams[ad_idx].cwnd = max(2 * MAX_MTU_SIZE, (fc->cparams[ad_idx].cwnd) / 2);
        event_logii(INTERNAL_EVENT_0,
                    "updating cwnd[%d], setting it to : %d\n", ad_idx, fc->cparams[ad_idx].cwnd);
        /* is this call inside the if, or outside the if ???? */
        /* do these pointers survive ? */
        /* timer is only restarted now, if list is empty */
        fc->cwnd_timer = adl_startTimer(rto, &fc_timer_cb_reduce_cwnd,TIMER_TYPE_CWND, assoc, data2);
    }
    /* ------------------ DEBUGGING ----------------------------- */
    /* fc_debug_cparams(VERBOSE);	*/
    /* ------------------ DEBUGGING ----------------------------- */
    mdi_clearAssociationData();
    return;
}

unsigned int fc_getNextActivePath(fc_data* fc, unsigned int start)
{
    unsigned int count = 0, path = start;
    while (count < fc->number_of_addresses) {
        path = (path+1)%fc->number_of_addresses;
        count++;
        if (pm_readState(path) == PM_ACTIVE) return path;
    }
    return path;
}

/**
 * function that selects destination index for data chunks when they are sent,
 * or possibly new address when they are retransmitted.
 * @param  fc   pointer to the flow control structure
 * @param  dat  pointer to the data chunk that is to be sent
 * @param  data_retransmitted   has the chunk already been transmitted ?
 * @param  old_destination      if so, we pass a pointer to the index of the last address used, else NULL
 * @return index of the address where we should send this chunk to, now
 */
unsigned int
fc_select_destination(fc_data * fc, chunk_data * dat,
                      boolean data_retransmitted, unsigned int *old_destination)
{
    /* TODO : check for number_of_addresses == 1, ==2 */
    unsigned int next = pm_readPrimaryPath();

    event_logiii(VVERBOSE, "fc_select_destination: chunk-tsn=%u, retrans=%s, primary path=%d ",
                 dat->chunk_tsn, ((data_retransmitted == TRUE) ? "TRUE" : "FALSE"), next);

    if (old_destination) {
        event_logi(VERBOSE, "fc_select_destination: old_dest = %u\n", *old_destination);
    } else {
        event_log(VERBOSE, "fc_select_destination: old_dest = NULL Pointer \n");
    }
    /* 1. return  a value that is equal to old_destination, if possible */
    if (old_destination) {
        if (pm_readState(*old_destination) == PM_ACTIVE) {
            return *old_destination;
        } else {
            return (fc_getNextActivePath(fc, *old_destination));
        }
    }
    /* 2. try user selected address */
    if (dat->initial_destination != -1) {
        next = (short) dat->initial_destination;
    }
    /* 3. else try the primary */
    if ((data_retransmitted == FALSE) && (pm_readState(next) == PM_ACTIVE))
        return next;
    /* 4. send retransmitted chunks to the next possible address */
    if (data_retransmitted == TRUE) next = dat->last_destination;

    return (fc_getNextActivePath(fc, next));
}


/**
 *  timer controlled callback function, called when T3 timer expires and data must be retransmitted
 *  This timer also adjusts the slow start threshold and cwnd values
 *  As all timer callbacks, it takes three arguments, the timerID, and two pointers to relevant data
 *  @param  tid the id of the timer that has gone off
 *  @param  assoc  pointer to the association structure to which this T3 timer belongs
 *  @param  data2  pointer to the index of the address where  T3 timer had been running
 */
void fc_timer_cb_t3_timeout(TimerID tid, void *assoc, void *data2)
{
    fc_data *fc;
    unsigned int ad_idx, res, retransmitted_bytes = 0;
    unsigned int oldListLen;
    int count;
    int num_of_chunks;
    chunk_data **chunks;
    gboolean removed_association = FALSE;

    res = mdi_setAssociationData(*(unsigned int *) assoc);
    if (res == 1) {
        error_log(ERROR_MAJOR, " association does not exist !");
        return;
    }
    if (res == 2) {
        error_log(ERROR_MAJOR, "Association was not cleared..... !!!");
        /* failure treatment ? */
    }

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return;
    }
    ad_idx = *((unsigned int *) data2);

    event_logi(INTERNAL_EVENT_0, "===============> fc_timer_cb_t3_timeout(address=%u) <========", ad_idx);

    fc->T3_timer[ad_idx] = 0;

    num_of_chunks = rtx_readNumberOfUnackedChunks();
    event_logii(INTERNAL_EVENT_0, "Address-Index : %u, Number of Chunks==%d", ad_idx, num_of_chunks);

    if (num_of_chunks <= 0) {
        event_log(VERBOSE, "Number of Chunks was 0 BEFORE calling rtx_t3_timeout - returning");
        if (fc->shutdown_received == TRUE)
            error_log(ERROR_MAJOR,
                      "T3 Timeout with 0 chunks in rtx-queue,  sci_allChunksAcked() should have been called !");
        mdi_clearAssociationData();
        return;
    }
    chunks = malloc(num_of_chunks * sizeof(chunk_data *));
    num_of_chunks = rtx_t3_timeout(&(fc->my_association), ad_idx, fc->cparams[ad_idx].mtu, chunks);
    if (num_of_chunks <= 0) {
        event_log(VERBOSE, "No Chunks to re-transmit - AFTER calling rtx_t3_timeout - returning");
        free(chunks);
        mdi_clearAssociationData();
        return;
    }
    oldListLen = fc->list_length;

    /* adjust ssthresh, cwnd - section 6.3.3.E1, respectively 7.2.3) */
    /* basically we halve the ssthresh, and set cwnd = mtu */
    fc->cparams[ad_idx].ssthresh = max(fc->cparams[ad_idx].cwnd / 2, 2 * fc->cparams[ad_idx].mtu);
    fc->cparams[ad_idx].cwnd = fc->cparams[ad_idx].mtu;

    for (count = 0; count < num_of_chunks; count++) {
        retransmitted_bytes += chunks[count]->chunk_len;
        event_logi(VERBOSE, "fc_timer_cb_t3_timeout: Got TSN==%u for RTXmit\n",
                   chunks[count]->chunk_tsn);

    }

    if (fc->outstanding_bytes >= retransmitted_bytes)
        fc->outstanding_bytes -= retransmitted_bytes;
    else
        fc->outstanding_bytes = 0;

    if (fc->cparams[ad_idx].outstanding_bytes_per_address >= retransmitted_bytes)
        fc->cparams[ad_idx].outstanding_bytes_per_address -= retransmitted_bytes;
    else
        fc->cparams[ad_idx].outstanding_bytes_per_address = 0;

    /* insert chunks to be retransmitted at the beginning of the list */
    /* make sure, that they are unique in this list ! */

    for (count = num_of_chunks - 1; count >= 0; count--) {
        if (g_list_find(fc->chunk_list, chunks[count]) == NULL){
            fc->chunk_list = g_list_insert_sorted(fc->chunk_list, chunks[count], (GCompareFunc) sort_tsn);
            fc->list_length++;
          } else {
            event_logi(VERBOSE, "Chunk number %u already in list, skipped adding it", chunks[count]->chunk_tsn);
        }

    }
    event_log(VVERBOSE, "\n-----FlowControl (T3 timeout): Chunklist after reinserting chunks -------");
    chunk_list_debug(VVERBOSE, fc->chunk_list);
    fc_debug_cparams(VVERBOSE);
    event_log(VVERBOSE, "-----FlowControl (T3 timeout): Debug Output End -------\n");
    free(chunks);

    /* section 7.2.3 : assure that only one data packet is in flight, until a new sack is received */
    fc->waiting_for_sack        = TRUE;
    fc->t3_retransmission_sent  = FALSE; /* we may again send one packet ! */
    fc->one_packet_inflight     = FALSE;

    removed_association = pm_chunksRetransmitted((short) ad_idx);

    if (removed_association) {
        event_log(INTERNAL_EVENT_0, "fc_timer_cb_t3_timeout: Association was terminated by pm_chunksRetransmitted()");
        mdi_clearAssociationData();
        return;
    }
    pm_rto_backoff(ad_idx);

    fc_check_for_txmit(fc, oldListLen);

    mdi_clearAssociationData();
    return;
}

/**
 * function increases chunk's number of transmissions, stores used destination, updates counts per addresses
 */
void fc_update_chunk_data(fc_data * fc, chunk_data * dat, unsigned int destination)
{
    unsigned int rwnd, last_destination;

    rwnd = rtx_read_remote_receiver_window();
    dat->num_of_transmissions++;

    event_logiii(VERBOSE,
                 "fc_update_chunk_data(),dat->TSN=%u, dat->num_of_transmissions %d , dest %d\n",
                 dat->chunk_tsn, dat->num_of_transmissions, destination);

    if (dat->num_of_transmissions == 1) {
        dat->last_destination = destination;
    }else if (dat->num_of_transmissions >= MAX_DEST) {
        error_log(ERROR_MINOR, "Maximum number of assumed transmissions exceeded ");
        dat->num_of_transmissions = MAX_DEST - 1;
    } else if (dat->num_of_transmissions < 1) {
        error_log(ERROR_FATAL, "Somehow dat->num_of_transmissions became less than 1 !");
        exit(-1);
    }

    /* this time we will send dat to destination */
    last_destination =  dat->last_destination;
    dat->last_destination = destination;

    /* if (dat->num_of_transmissions == 1)*/
    /* chunk is being transmitted the first time */
    /* outstanding byte counter has been decreased if chunks were scheduled for RTX, increase here ! */
    fc->outstanding_bytes += dat->chunk_len;

    if (dat->num_of_transmissions <= 1) { /* chunk is transmitted for the first time */
        fc->cparams[destination].outstanding_bytes_per_address += dat->chunk_len;
    }
    /* section 6.2.1.B */
    /* leave peers arwnd untouched for retransmitted data !!!!!!!!! */
    if (dat->num_of_transmissions == 1) {
        if (dat->chunk_len >= rwnd)
            rtx_set_remote_receiver_window(0);
        else
            rtx_set_remote_receiver_window(rwnd - dat->chunk_len);
    }

    event_logiii(VERBOSE, "outstanding_bytes_per_address(%u)=%u bytes, Overall: %u",
                destination, fc->cparams[destination].outstanding_bytes_per_address,
                fc->outstanding_bytes);
    return;
}


/**
 *  function that checks whether we may transmit data that is currently in the send queue.
 *  Any time that some data chunk is added to the send queue, we must check, whether we can send
 *  the chunk, or must wait until cwnd opens up.
 *  @param fc_instance  pointer to the flowcontrol instance used here
 *  @return  0 for successful send event, -1 for error, 1 if nothing was sent
 */
int fc_check_for_txmit(void *fc_instance, unsigned int oldListLen)
{
    int result, len;
    fc_data *fc;
    chunk_data *dat;
    unsigned int total_size, destination, chunk_len, peer_rwnd, rto_time;
    unsigned int obpa;
    gboolean data_is_retransmitted = FALSE;
    gboolean lowest_tsn_is_retransmitted = FALSE;
    gboolean data_is_submitted = FALSE;
    peer_rwnd = rtx_read_remote_receiver_window();
    event_logi(INTERNAL_EVENT_0, "Entering fc_check_for_txmit(rwnd=%u)... ", peer_rwnd);

    fc = (fc_data *) fc_instance;
    /* ------------------ DEBUGGING ----------------------------- */
    /*  chunk_list_debug(VVERBOSE, fc->chunk_list); */
    /* fc_debug_cparams(VERBOSE);                           */
    /* ------------------ DEBUGGING ----------------------------- */

    if (fc->chunk_list != NULL) {
          dat = g_list_nth_data(fc->chunk_list, 0);
    } else {
        return -1;
    }

    if (dat->num_of_transmissions >= 1)  data_is_retransmitted = TRUE;

    destination = fc_select_destination(fc, dat, data_is_retransmitted, NULL);

    total_size = 0;
    chunk_len = dat->chunk_len;

    event_logiii(VERBOSE, "Called fc_select_destination == %d, chunk_len=%u, outstanding(p.add)=%u",
                 destination, chunk_len, fc->cparams[destination].outstanding_bytes_per_address);
    event_logiiii(VERBOSE, "cwnd(%u) == %u, mtu == %u, MAX_MTU = %d ",
                  destination, fc->cparams[destination].cwnd, fc->cparams[destination].mtu, MAX_MTU_SIZE);

    if (peer_rwnd == 0 && fc->one_packet_inflight == TRUE) {    /* section 6.1.A */
            event_log(VERBOSE, "NOT SENDING (peer rwnd == 0 and already one packet in flight ");
            return 1;
    }

    if (fc->cparams[destination].cwnd <= fc->cparams[destination].outstanding_bytes_per_address) {
        event_logiii(VERBOSE, "NOT SENDING (cwnd=%u, outstanding(%u)=%u)",
                     fc->cparams[destination].cwnd, destination,
                     fc->cparams[destination].outstanding_bytes_per_address);
        return 1;
    }

    if (fc->waiting_for_sack == TRUE && data_is_retransmitted == TRUE) {
        /* make sure we send only one retransmission after T3 timeout */
        if (fc->t3_retransmission_sent == TRUE) {
            event_log(VERBOSE, "########################################## -> Returned in fc_check_for_txmit");
            return 1;
        }
    }

    obpa = fc->cparams[destination].outstanding_bytes_per_address;

    while ((dat != NULL) &&
           (total_size + obpa < (fc->cparams[destination].cwnd+fc->cparams[destination].mtu-1)) &&
           (
            ((dat->num_of_transmissions==0)&&(rtx_read_remote_receiver_window() > chunk_len)) ||
            (rtx_read_remote_receiver_window()==0 && fc->one_packet_inflight == FALSE) ||
            (dat->num_of_transmissions > 0)) ) {

        /* size is used to see, whether we may send this next chunk, too */
        total_size += dat->chunk_len;

        event_logiii(VVERBOSE, "Chunk: len=%u, tsn=%u, gap_reports=%u",
                     dat->chunk_len, dat->chunk_tsn, dat->gap_reports);
        event_logii(VVERBOSE, "Chunk: ack_time=%d, num_of_transmissions=%u",
                    dat->ack_time, dat->num_of_transmissions);

        result = bu_put_Data_Chunk((SCTP_simple_chunk *) dat->data, &destination);
        data_is_submitted = TRUE;

        event_logi(VERBOSE, "sent chunk (tsn=%u) to bundling", dat->chunk_tsn);
        event_log(VVERBOSE, "Calling fc_update_chunk_data \n");

        fc_update_chunk_data(fc, dat, destination);
        if (dat->num_of_transmissions == 1) {
            adl_gettime(&(dat->transmission_time));
            event_log(INTERNAL_EVENT_0, "Storing chunk in retransmission list -> calling rtx_save_retrans");
            result = rtx_save_retrans_chunks(dat);
        } else {
            if (lowest_tsn_is_retransmitted == FALSE)
                /* must not be reset to FALSE here */
                lowest_tsn_is_retransmitted = rtx_is_lowest_tsn(dat->chunk_tsn);
        }
        fc->one_packet_inflight = TRUE;
        fc->chunk_list = g_list_remove(fc->chunk_list, (gpointer) dat);
        fc->list_length--;
        dat = g_list_nth_data(fc->chunk_list, 0);

        if (dat != NULL) {
            if (dat->num_of_transmissions >= 1)  data_is_retransmitted = TRUE;
            else if (dat->num_of_transmissions == 0) data_is_retransmitted = FALSE;
            chunk_len = dat->chunk_len;
            destination = fc_select_destination(fc, dat, data_is_retransmitted, &destination);
            event_logi(VERBOSE, "Called fc_select_destination == %d\n", destination);

            if ((rtx_read_remote_receiver_window() < chunk_len) && data_is_retransmitted == FALSE) {
                break;
            }

        }     /* if (dat != NULL) */

    }  /* while ((dat != NULL) && */


    if ((fc->waiting_for_sack == TRUE) && (fc->t3_retransmission_sent == FALSE)) {
        if (data_is_submitted == TRUE && data_is_retransmitted == TRUE) {
            event_log(VERBOSE, "Retransmission Condition in fc_check_for_txmit !!!!!!!! ");
            /* Keep me from retransmitting more than once */
            fc->t3_retransmission_sent = TRUE;
        }
    }

    /* ------------------ DEBUGGING ----------------------------- */
    event_log(VVERBOSE, "Printing Chunk List / Congestion Params in fc_check_for_txmit");
    chunk_list_debug(VVERBOSE, fc->chunk_list);
    /* fc_debug_cparams(VVERBOSE);*/
    /* ------------------ DEBUGGING ----------------------------- */

    if (fc->T3_timer[destination] == 0) { /* see section 5.1 */
        fc->T3_timer[destination] =
            adl_startTimer(pm_readRTO(destination), &fc_timer_cb_t3_timeout,TIMER_TYPE_RTXM,
                            &(fc->my_association), &(fc->addresses[destination]));
        event_logiii(INTERNAL_EVENT_0,
                     "fc_check_for_transmit: started T3 Timer with RTO(%u)==%u msecs on address %u",
                     destination, pm_readRTO(destination), fc->addresses[destination]);
    } else {
        /* restart only if lowest TSN is being retransmitted, else leave running */
        /* see section 6.1 */
        if (lowest_tsn_is_retransmitted) {
            event_logiii(INTERNAL_EVENT_0,
                         "RTX of lowest TSN: Restarted T3 Timer with RTO(%u)==%u msecs on address %u",
                         destination, pm_readRTO(destination), fc->addresses[destination]);
            fc->T3_timer[destination] =
                adl_restartTimer(fc->T3_timer[destination], pm_readRTO(destination));
        }
    }

    len = fc->list_length;

    if (data_is_submitted == TRUE) {
        fc->one_packet_inflight = TRUE;
        bu_sendAllChunks(&destination);
        rto_time = pm_readRTO(destination);
        fc->last_active_address = destination;

        if (fc->cwnd_timer == 0) {
            fc->cwnd_timer =
                adl_startTimer(rto_time, &fc_timer_cb_reduce_cwnd, TIMER_TYPE_CWND, &(fc->my_association), NULL);
            event_logi(INTERNAL_EVENT_0,
                       "fc_check_for_txmit...started reduce-cwnd-Timer going off %u msecs from now",
                       rto_time);
        } else {
            fc->cwnd_timer = adl_restartTimer(fc->cwnd_timer, rto_time);
            event_logi(INTERNAL_EVENT_0,
                       "fc_check_for_txmit...re-started reduce-cwnd-Timer going off %u msecs from now",
                       rto_time);
        }
        if (fc->maxQueueLen != 0) {
            if (len < fc->maxQueueLen && oldListLen >= fc->maxQueueLen) {
                 mdi_queueStatusChangeNotif(SCTP_SEND_QUEUE, 0, len);
            } else if (len > fc->maxQueueLen && oldListLen <= fc->maxQueueLen) {
                 mdi_queueStatusChangeNotif(SCTP_SEND_QUEUE, 0, len);
            }
        }
        return 0;
    }

    if (fc->maxQueueLen != 0) {
        if (len < fc->maxQueueLen && oldListLen >= fc->maxQueueLen) {
            mdi_queueStatusChangeNotif(SCTP_SEND_QUEUE, 0, len);
        } else if (len > fc->maxQueueLen && oldListLen <= fc->maxQueueLen) {
             mdi_queueStatusChangeNotif(SCTP_SEND_QUEUE, 0, len);
        }
    }

    return 1;
}

/*
  this function checks whether T3 may be stopped, restarted or left running
  @param ad_idx  index of the destination address concerned (which may have a T3 timer running)
  @param all_acked   has all data been acked ?
  @param new_acked   have new chunks been acked ? CHECKME : has the ctsna advanced ?
*/
void fc_check_t3(unsigned int ad_idx, unsigned int obpa, boolean all_acked, boolean new_acked)
{
    fc_data *fc;
    int result,count;
    fc = (fc_data *) mdi_readFlowControl();

    event_logiii(INTERNAL_EVENT_0,
                 "fc_check_t3(%u,all_acked=%s, new_acked=%s)... ", ad_idx,
                 (all_acked == TRUE) ? "true" : "false", (new_acked == TRUE) ? "true" : "false");

    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return;
    }
    if (all_acked == TRUE) {
        for (count = 0; count < fc->number_of_addresses; count++) {
            if (fc->T3_timer[count] != 0) {
                result = sctp_stopTimer(fc->T3_timer[count]);
                event_logii(INTERNAL_EVENT_0, "Stopped T3 Timer(%d), Result was %d ", count, result);
                fc->T3_timer[count] = 0;
            }
        }
        return;
    }
    /* see section 6.3.2.R2 */
    if (obpa == 0) {
        if (fc->T3_timer[ad_idx] != 0) {
            result = sctp_stopTimer(fc->T3_timer[ad_idx]);
            event_logii(INTERNAL_EVENT_0, "Stopped T3 Timer(%u), Result was %d ", ad_idx, result);
            fc->T3_timer[ad_idx] = 0;
        }
        return;
    }
    /*
     *  6.3.2 R3) Whenever a SACK is received that acknowledges new data chunks
     *  including the one with the earliest outstanding TSN on that address,
     *  restart T3-rxt timer of that address with its current RTO. (if there is
     *  still data outstanding on that address
     */

    if (new_acked == TRUE) {
        /* 6.2.4.4) Restart T3, if SACK acked lowest outstanding tsn, OR
         *                      we are retransmitting the first outstanding data chunk
         */
        if (fc->T3_timer[ad_idx] != 0) {
            fc->T3_timer[ad_idx] =
                adl_restartTimer(fc->T3_timer[ad_idx], pm_readRTO(ad_idx));
            event_logii(INTERNAL_EVENT_0,
                        "Restarted T3 Timer with RTO==%u msecs on address %u",
                        pm_readRTO(ad_idx), ad_idx);
        } else {
            fc->T3_timer[ad_idx] =
                adl_startTimer(pm_readRTO(ad_idx), &fc_timer_cb_t3_timeout, TIMER_TYPE_RTXM,
                                &(fc->my_association), &(fc->addresses[ad_idx]));
            event_logii(INTERNAL_EVENT_0,
                        "Started T3 Timer with RTO==%u msecs on address %u",
                        pm_readRTO(ad_idx), ad_idx);

        }
        return;
    }
    event_log(INTERNAL_EVENT_0, "Left T3 Timer running...");
    /* else leave T3 running  */
    return;
}

/**
 * Function called by stream engine to enqueue data chunks in the flowcontrol
 * module. After function returns, we should be able to  delete the pointer
 * to the data (i.e. some lower module must have copied the data...e.g. the
 * Flowcontrol, ReliableTransfer, or Bundling
 * @param  chunk    pointer to the data chunk to be sent
 * @param destAddressIndex index to address to send data structure to...
 * @param  lifetime NULL if unused, else pointer to a value of msecs,
           after which data will not be sent anymore
 * @param   dontBundle NULL if unused, by default bundling is allowed,
            else pointer to boolean indicating whether it is or it is not allowed.
 * @return -1 on error, 0 on success, (1 if problems occurred ?)
 */
int fc_send_data_chunk(chunk_data * chunkd,
                       short destAddressIndex,
                       unsigned int lifetime,
                       gboolean dontBundle,
                       gpointer context)
{
    fc_data *fc=NULL;

    SCTP_data_chunk* s_chunk;

    event_log(INTERNAL_EVENT_0, "fc_send_data_chunk is being executed.");

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }

    if (fc->shutdown_received == TRUE) {
        error_log(ERROR_MAJOR,
                  "fc_send_data_chunk() called, but shutdown_received==TRUE - send not allowed !");
        free(chunkd);
        /* FIXME: see that error treatment gives direct feedback of  this to the ULP ! */
        return SCTP_SPECIFIC_FUNCTION_ERROR;
    }

    /* event_log(VVERBOSE, "Printing Chunk List / Congestion Params in fc_send_data_chunk - before");
    chunk_list_debug(VVERBOSE, fc->chunk_list); */

    event_log(VERBOSE, "FlowControl got a Data Chunk to send ");

    s_chunk = (SCTP_data_chunk*)chunkd->data;

    /* early TSN assignment */
    s_chunk->tsn        =  htonl(fc->current_tsn++);
    chunkd->chunk_len   = CHUNKP_LENGTH(s_chunk);
    chunkd->chunk_tsn   = ntohl(s_chunk->tsn);
    chunkd->gap_reports = 0L;
    chunkd->ack_time    = 0;
    chunkd->context     = context;
    chunkd->hasBeenAcked= FALSE;
    chunkd->hasBeenDropped = FALSE;
    chunkd->last_destination = 0;

    if (destAddressIndex >= 0) chunkd->initial_destination = destAddressIndex;
    else chunkd->initial_destination = -1;

    if (lifetime == 0xFFFFFFFF) {
        timerclear(&(chunkd->expiry_time));
    } else if (lifetime == 0) {
        adl_gettime(&(chunkd->expiry_time));
    } else {
        adl_gettime(&(chunkd->expiry_time));
        adl_add_msecs_totime(&(chunkd->expiry_time), lifetime);
    }

    timerclear(&(chunkd->transmission_time));

    chunkd->dontBundle           = dontBundle;
    chunkd->num_of_transmissions = 0;

    /* insert chunk at the list's tail */
    fc->chunk_list = g_list_append(fc->chunk_list, chunkd);
    fc->list_length++;
    event_log(VVERBOSE, "Printing Chunk List / Congestion Params in  fc_send_data_chunk - after");
    chunk_list_debug(VVERBOSE, fc->chunk_list);

    fc_check_for_txmit(fc, fc->list_length);

    return SCTP_SUCCESS;
}


int fc_dequeue_acked_chunks(unsigned int ctsna)
{
    chunk_data *dat = NULL;
    GList* tmp = NULL;
    fc_data *fc = NULL;

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_FATAL, "fc_data instance not set in fc_dequeue_acked_chunks !");
        return (-1);
    }

    tmp = g_list_first(fc->chunk_list);

    while (tmp != NULL) {
        dat = (chunk_data*)tmp->data;
         if (before(dat->chunk_tsn, ctsna) || (dat->chunk_tsn == ctsna)) {
            tmp = g_list_next(tmp);
            fc->chunk_list = g_list_remove(fc->chunk_list, (gpointer) dat);
            fc->list_length--;
            event_logii(INTERNAL_EVENT_0, "Removed chunk %u from Flowcontrol-List, Listlength now %u",
                dat->chunk_tsn, fc->list_length);
        } else
            break;
    }

    return 0;
}

/**
 * function called by Reliable Transfer, when it requests retransmission
 * in SDL diagram this signal is called (Req_RTX, RetransChunks)
 * @param  all_data_acked indicates whether or not all data chunks have been acked
 * @param   new_data_acked indicates whether or not new data has been acked
 * @param   num_acked number of bytes that have been newly acked, else 0
 * @param   number_of_addresses so many addresses may have outstanding bytes
 *          actually that value may also be retrieved from the association struct (?)
 * @param   num_acked_per_address array of integers, that hold number of bytes acked for each address
 * @param   number_of_rtx_chunks number indicatin, how many chunks are to be retransmitted in on datagram
 * @param   chunks  array of pointers to data_chunk structures. These are to be retransmitted
 * @return   -1 on error, 0 on success, (1 if problems occurred ?)
 */
int fc_fast_retransmission(unsigned int address_index, unsigned int arwnd, unsigned int ctsna,
                     unsigned int rtx_bytes, boolean all_data_acked,
                     boolean new_data_acked, unsigned int num_acked,
                     unsigned int number_of_addresses,
                     unsigned int *num_acked_per_address,
                     int number_of_rtx_chunks, chunk_data ** chunks)
{
    fc_data *fc;
    int count, result;
    unsigned int oldListLen;

    unsigned rtt_time, peer_rwnd;
    struct timeval last_update, now;
//    chunk_data temp;
    int diff;

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return (-1);
    }

    oldListLen = fc->list_length;

    /* ------------------ DEBUGGING ----------------------------- */
    fc_debug_cparams(VERBOSE);
    /* ------------------ DEBUGGING ----------------------------- */

    fc->t3_retransmission_sent = FALSE; /* we have received a SACK, so reset this */
    /* if (num_acked > 0) */
    /* just checking if the other guy is still alive */
        fc->waiting_for_sack = FALSE;

    for (count = 0; count < number_of_rtx_chunks; count++)
        event_logi(VERBOSE, "fc_fast_retransmission: Got TSN==%u for RTXmit\n",
                   chunks[count]->chunk_tsn);


    /* see section 6.2.1, section 6.2.2 */
    if (fc->cparams[address_index].cwnd <= fc->cparams[address_index].ssthresh) { /* SLOW START */
        for (count = 0; count < number_of_addresses; count++) {
            event_logii(VERBOSE, "SLOW START : outstanding bytes on(%d)=%u ",
                        count, fc->cparams[count].outstanding_bytes_per_address);
            event_logii(VERBOSE, "SLOW START : numacked per address(%d)=%u ",
                        count, num_acked_per_address[count]);
            fc->cparams[count].partial_bytes_acked = 0;
        }
	    fc->outstanding_bytes =
    	    (fc->outstanding_bytes <= num_acked) ? 0 : (fc->outstanding_bytes - num_acked);
    	for (count = 0; count < number_of_addresses; count++) {
        	fc->cparams[count].outstanding_bytes_per_address =
            	(fc->cparams[count].outstanding_bytes_per_address <
             	num_acked_per_address[count]) ? 0 : (fc->cparams
                                                  [count].outstanding_bytes_per_address
                                                  - num_acked_per_address[count]);
    	}
        if (new_data_acked == TRUE) {
            fc->cparams[address_index].cwnd += min(MAX_MTU_SIZE, num_acked);
        }
    } else {                    /* CONGESTION AVOIDANCE, as per section 6.2.2 */
        if (new_data_acked == TRUE) {
            fc->cparams[address_index].partial_bytes_acked += num_acked;
            event_logii(VVERBOSE, "CONG. AVOIDANCE : (fast RTX) -> new data acked: increase PBA(%u) to %u",
                address_index,fc->cparams[address_index].partial_bytes_acked);
        }

        rtt_time = pm_readSRTT(address_index);
        memcpy(&last_update, &(fc->cparams[address_index].time_of_cwnd_adjustment), sizeof(struct timeval));
        adl_add_msecs_totime(&last_update, rtt_time);
        adl_gettime(&now);
        diff = adl_timediff_to_msecs(&now, &last_update); /* a-b */
        event_logii(VVERBOSE, "CONG. AVOIDANCE : rtt_time=%u diff=%d", rtt_time, diff);

        if (diff >= 0) {
            if ((fc->cparams[address_index].partial_bytes_acked >= fc->cparams[address_index].cwnd)
                && (fc->outstanding_bytes >= fc->cparams[address_index].cwnd)) {
                fc->cparams[address_index].cwnd += MAX_MTU_SIZE;
                fc->cparams[address_index].partial_bytes_acked -= fc->cparams[address_index].cwnd;
                /* update time of window adjustment (i.e. now) */
                event_log(VVERBOSE,
                          "CONG. AVOIDANCE : updating time of adjustment !!!!!!!!!! NOW ! ");
            }
            event_logii(VERBOSE,
                        "CONG. AVOIDANCE : updated counters: %u bytes outstanding, cwnd=%u",
                        fc->outstanding_bytes, fc->cparams[address_index].cwnd);
        }
	    fc->outstanding_bytes =
    	    (fc->outstanding_bytes <= num_acked) ? 0 : (fc->outstanding_bytes - num_acked);
    	for (count = 0; count < number_of_addresses; count++) {
        	fc->cparams[count].outstanding_bytes_per_address =
            	(fc->cparams[count].outstanding_bytes_per_address <
            	 num_acked_per_address[count]) ? 0 : (fc->cparams
                                                  [count].outstanding_bytes_per_address
                                                  - num_acked_per_address[count]);
    	}

        event_logii(VVERBOSE, "CONG. AVOIDANCE : partial_bytes_acked(%u)=%u ",
                    address_index, fc->cparams[address_index].partial_bytes_acked);

        for (count = 0; count < number_of_addresses; count++) {
            event_logii(VVERBOSE,
                        "CONG. AVOIDANCE : outstanding bytes on(%d)=%u ",
                        count, fc->cparams[count].outstanding_bytes_per_address);
            event_logii(VVERBOSE,
                        "CONG. AVOIDANCE : numacked per address(%d)=%u ",
                        count, num_acked_per_address[count]);
        }
        /* see section 7.2.2 */
        if (all_data_acked == TRUE)
            fc->cparams[address_index].partial_bytes_acked = 0;

    }
    result = -2;
    /* We HAVE retransmission, so DO UPDATE OF WINDOW PARAMETERS , see section 6.2.3 */
    fc->cparams[address_index].ssthresh =
        max(fc->cparams[address_index].cwnd / 2, 2 * fc->cparams[address_index].mtu);
    fc->cparams[address_index].cwnd = fc->cparams[address_index].ssthresh;
    event_logiiii(VERBOSE,
                  "fc_fast_retransmission: updated: %u bytes outstanding, %u bytes per address, cwnd=%u, ssthresh=%u",
                  fc->outstanding_bytes,
                  fc->cparams[address_index].outstanding_bytes_per_address,
                  fc->cparams[address_index].cwnd, fc->cparams[address_index].ssthresh);

    /* This is to be an ordered list containing no duplicate entries ! */
    for (count = number_of_rtx_chunks - 1; count >= 0; count--) {

        if (g_list_find(fc->chunk_list, chunks[count]) != NULL){
            event_logii(VERBOSE,
                        "chunk_tsn==%u, count==%u already in the list -- continue with next\n",
                        chunks[count]->chunk_tsn, count);
            continue;
        }
        event_logii(INTERNAL_EVENT_0,
                    "inserting chunk_tsn==%u, count==%u in the list\n",
                    chunks[count]->chunk_tsn, count);

        fc->chunk_list = g_list_insert_sorted(fc->chunk_list, chunks[count], (GCompareFunc) sort_tsn);
        fc->list_length++;
    }
    fc->last_active_address = address_index;

    event_log(VVERBOSE, "fc_fast_retransmission: FlowControl Chunklist after Re-Insertion \n");
    chunk_list_debug(VVERBOSE, fc->chunk_list);

    fc_check_t3(address_index, fc->cparams[address_index].outstanding_bytes_per_address,
                all_data_acked, new_data_acked);


    /* section 6.2.1.D ?? */
    if (arwnd >= fc->outstanding_bytes) {
        peer_rwnd = arwnd - fc->outstanding_bytes;
    } else {
        peer_rwnd = 0;
    }
    /* section 6.2.1.C */
    rtx_set_remote_receiver_window(peer_rwnd);


    if (fc->outstanding_bytes >= rtx_bytes)
        fc->outstanding_bytes -= rtx_bytes;
    else
        fc->outstanding_bytes = 0;

    if (fc->cparams[address_index].outstanding_bytes_per_address >= rtx_bytes)
        fc->cparams[address_index].outstanding_bytes_per_address -= rtx_bytes;
    else
        fc->cparams[address_index].outstanding_bytes_per_address = 0;

    if (fc->outstanding_bytes==0) {
        fc->one_packet_inflight = FALSE;
    } else {
        fc->one_packet_inflight = TRUE;
    }

    /* send as many to bundling as allowed, requesting new destination address */
    if (fc->chunk_list != NULL){
       result = fc_check_for_txmit(fc, oldListLen);
    }
    /* make sure that SACK chunk is actually sent ! */
    if (result != 0) bu_sendAllChunks(NULL);

    adl_gettime(&(fc->cparams[address_index].time_of_cwnd_adjustment));

    return 1;
}     /* end: fc_fast_retransmission */


/**
 * function called by Reliable Transfer, after it has got a SACK chunk
 * in SDL diagram this signal is called SACK_Info
 * @param  all_data_acked indicates whether or not all data chunks have been acked
 * @param   new_data_acked indicates whether or not new data has been acked
 * @param   num_acked number of bytes that have been newly acked, else 0
 * @param   number_of_addresses so many addresses may have outstanding bytes
 *          actually that value may also be retrieved from the association struct (?)
 * @param   num_acked_per_address array of integers, that hold number of bytes acked for each address
 */
void fc_sack_info(unsigned int address_index, unsigned int arwnd,unsigned int ctsna,
             boolean all_data_acked, boolean new_data_acked,
             unsigned int num_acked, unsigned int number_of_addresses,
             unsigned int *num_acked_per_address)
{
    fc_data *fc;
    unsigned int count, rto_time, rtt_time;
    struct timeval last_update, now;
    int diff;
    unsigned int oldListLen;

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "fc_data instance not set !");
        return;
    }
    /* ------------------ DEBUGGING ----------------------------- */
    fc_debug_cparams(VERBOSE);
    /* ------------------ DEBUGGING ----------------------------- */

    event_logii(INTERNAL_EVENT_0,
                "fc_sack_info...bytes acked=%u on address %u ", num_acked, address_index);
    fc->t3_retransmission_sent = FALSE; /* we have received a SACK, so reset this */

    /* just check that the other guy is still alive */
    fc->waiting_for_sack = FALSE;

    oldListLen = fc->list_length;

    /* see section 6.2.1, section 6.2.2 */
    if (fc->cparams[address_index].cwnd <= fc->cparams[address_index].ssthresh) { /* SLOW START */
        for (count = 0; count < number_of_addresses; count++) {
            event_logii(VERBOSE, "SLOW START : outstanding bytes on(%d)=%u ",
                        count, fc->cparams[count].outstanding_bytes_per_address);
            event_logii(VERBOSE, "SLOW START : numacked per address(%d)=%u ",
                        count, num_acked_per_address[count]);
            fc->cparams[count].partial_bytes_acked = 0;
  	        fc->cparams[count].outstanding_bytes_per_address =
    	        (fc->cparams[count].outstanding_bytes_per_address <
        	     num_acked_per_address[count]) ? 0 : (fc->cparams
                                                  [count].outstanding_bytes_per_address
                                                  - num_acked_per_address[count]);
        }
        if (new_data_acked == TRUE) {
            fc->cparams[address_index].cwnd += min(MAX_MTU_SIZE, num_acked);
            adl_gettime(&(fc->cparams[address_index].time_of_cwnd_adjustment));
        }
	    fc->outstanding_bytes =
    	    (fc->outstanding_bytes <= num_acked) ? 0 : (fc->outstanding_bytes - num_acked);
        event_logiii(VERBOSE,
                     "SLOW START : updated counters: TOTAL %u bytes outstanding, cwnd(%u)=%u",
                     fc->outstanding_bytes, address_index, fc->cparams[address_index].cwnd);
    } else {                    /* CONGESTION AVOIDANCE, as per section 6.2.2 */

        if (new_data_acked == TRUE) {
            fc->cparams[address_index].partial_bytes_acked += num_acked;
            event_logii(VVERBOSE, "CONG. AVOIDANCE : new data acked: increase PBA(%u) to %u",
                address_index,fc->cparams[address_index].partial_bytes_acked);
        }
        /*
         * Section 7.2.2 :
		 * "When partial_bytes_acked is equal to or greater than cwnd and
      	 * before the arrival of the SACK the sender had cwnd or more bytes
      	 * of data outstanding (i.e., before arrival of the SACK, flightsize
      	 * was greater than or equal to cwnd), increase cwnd by MTU, and
      	 * reset partial_bytes_acked to (partial_bytes_acked - cwnd)."
      	 */
        rtt_time = pm_readSRTT(address_index);
        memcpy(&last_update, &(fc->cparams[address_index].time_of_cwnd_adjustment), sizeof(struct timeval));
        adl_add_msecs_totime(&last_update, rtt_time);
        adl_gettime(&now);
        diff = adl_timediff_to_msecs(&now, &last_update); /* a-b */

        event_logii(VVERBOSE, "CONG. AVOIDANCE : rtt_time=%u diff=%d", rtt_time, diff);

        if (diff >= 0) { /* we may update cwnd once per SRTT */
            if ((fc->cparams[address_index].partial_bytes_acked >= fc->cparams[address_index].cwnd)
                && (fc->outstanding_bytes >= fc->cparams[address_index].cwnd)) {
                fc->cparams[address_index].cwnd += MAX_MTU_SIZE;
                fc->cparams[address_index].partial_bytes_acked -= fc->cparams[address_index].cwnd;
                event_log(VVERBOSE,
                          "CONG. AVOIDANCE : updating time of adjustment !!!!!!!!!! NOW ! ");
                adl_gettime(&(fc->cparams[address_index].time_of_cwnd_adjustment));
            }
            event_logii(VERBOSE,
                        "CONG. AVOIDANCE : updated counters: %u bytes outstanding, cwnd=%u",
                        fc->outstanding_bytes, fc->cparams[address_index].cwnd);
            /* update time of window adjustment (i.e. now) */
        }

   	    fc->outstanding_bytes =
    	    (fc->outstanding_bytes <= num_acked) ? 0 : (fc->outstanding_bytes - num_acked);

        event_logii(VVERBOSE, "CONG. AVOIDANCE : partial_bytes_acked(%u)=%u ",
                    address_index, fc->cparams[address_index].partial_bytes_acked);

        for (count = 0; count < number_of_addresses; count++) {
	        fc->cparams[count].outstanding_bytes_per_address =
    	        (fc->cparams[count].outstanding_bytes_per_address <
        	     num_acked_per_address[count]) ? 0 : (fc->cparams
                                                  [count].outstanding_bytes_per_address
                                                  - num_acked_per_address[count]);

            event_logii(VVERBOSE,
                        "CONG. AVOIDANCE : outstanding bytes on(%d)=%u ",
                        count, fc->cparams[count].outstanding_bytes_per_address);
            event_logii(VVERBOSE,
                        "CONG. AVOIDANCE : numacked per address(%d)=%u ",
                        count, num_acked_per_address[count]);
        }
        /* see section 7.2.2 */
        if (all_data_acked == TRUE) fc->cparams[address_index].partial_bytes_acked = 0;

    }
    /* if we don't send data, cwnd = max(cwnd/2, 2*MTU), once per RTO */
    rto_time = pm_readRTO(address_index);
    fc->last_active_address = address_index;

    if ((fc->cwnd_timer == 0) && (fc->chunk_list == NULL)) {
        fc->cwnd_timer =
            adl_startTimer(rto_time, &fc_timer_cb_reduce_cwnd,TIMER_TYPE_CWND, &(fc->my_association), NULL);
        event_logi(INTERNAL_EVENT_0,
                   "fc_sack_info...started reduce-cwnd-Timer going off %u msecs from now",
                   rto_time);
    }

    fc_check_t3(address_index, fc->cparams[address_index].outstanding_bytes_per_address,
                all_data_acked, new_data_acked);

    if (fc->outstanding_bytes == 0) {
        fc->one_packet_inflight = FALSE;
    } else {
        fc->one_packet_inflight = TRUE;
    }


    /* section 6.2.1.C */
    if (arwnd > fc->outstanding_bytes)
        rtx_set_remote_receiver_window(arwnd - fc->outstanding_bytes);
    else
        rtx_set_remote_receiver_window(0);

    if (fc->chunk_list != NULL) {
        fc_check_for_txmit(fc, oldListLen);
    }
    return;
}    /* end: fc_sack_info  */



int fc_dequeueUnackedChunk(unsigned int tsn)
{
    fc_data *fc = NULL;
    chunk_data *dat = NULL;
    GList *tmp = NULL;
    gboolean found = FALSE;
    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return SCTP_MODULE_NOT_FOUND;
    }
    dat = g_list_nth_data(fc->chunk_list, 0);
    tmp = fc->chunk_list;
    while (dat != NULL && tmp != NULL) {
        event_logii(VVERBOSE, "fc_dequeueOldestUnsentChunks(): checking chunk tsn=%u, num_rtx=%u ", dat->chunk_tsn, dat->num_of_transmissions);
        if (dat->chunk_tsn == tsn) {
            found = TRUE;
            break;
        } else {
            tmp = g_list_next(tmp);
            if (tmp != NULL) {
                dat = (chunk_data*)tmp->data;
            } else {
                dat = NULL;
            }
        }
    }
    if (found) { /* delete */
        fc->chunk_list = g_list_remove(fc->chunk_list, (gpointer) dat);
        fc->list_length--;
        event_log(VVERBOSE, "fc_dequeueUnackedChunk(): checking list");
        chunk_list_debug(VVERBOSE, fc->chunk_list);
        return 1;
    }
    /* else */
    return 0;


}

int fc_dequeueOldestUnsentChunk(unsigned char *buf, unsigned int *len, unsigned int *tsn,
                                unsigned short *sID, unsigned short *sSN,unsigned int* pID,
                                unsigned char* flags, gpointer* ctx)
{
    fc_data *fc = NULL;
    chunk_data *dat = NULL;
    GList *tmp = NULL;
    SCTP_data_chunk* dchunk;
    int listlen;

    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return SCTP_MODULE_NOT_FOUND;
    }
    listlen =  fc_readNumberOfUnsentChunks();

    if (listlen <= 0)               return SCTP_UNSPECIFIED_ERROR;
    if (fc->chunk_list == NULL) return  SCTP_UNSPECIFIED_ERROR;
    dat = g_list_nth_data(fc->chunk_list, 0);
    tmp = fc->chunk_list;
    while (dat != NULL && tmp != NULL) {
        event_logii(VVERBOSE, "fc_dequeueOldestUnsentChunks(): checking chunk tsn=%u, num_rtx=%u ", dat->chunk_tsn, dat->num_of_transmissions);
        if (dat->num_of_transmissions != 0) {
            tmp = g_list_next(tmp);
            dat = (chunk_data*)tmp->data;
        /* should be a sorted list, and not happen here */
        } else break;
    }
    if ((*len) <  (dat->chunk_len - FIXED_DATA_CHUNK_SIZE)) return SCTP_BUFFER_TOO_SMALL;

    event_logii(VVERBOSE, "fc_dequeueOldestUnsentChunks(): returning chunk tsn=%u, num_rtx=%u ", dat->chunk_tsn, dat->num_of_transmissions);

    dchunk = (SCTP_data_chunk*) dat->data;
    *len = dat->chunk_len - FIXED_DATA_CHUNK_SIZE;
    memcpy(buf, dchunk->data, dat->chunk_len - FIXED_DATA_CHUNK_SIZE);
    *tsn = dat->chunk_tsn;
    *sID = ntohs(dchunk->stream_id);
    *sSN = ntohs(dchunk->stream_sn);
    *pID = ntohl(dchunk->protocolId);
    *flags = dchunk->chunk_flags;
    *ctx = dat->context;
    fc->chunk_list = g_list_remove(fc->chunk_list, (gpointer) dat);
    fc->list_length--;
    /* be careful ! data may only be freed once: this module ONLY takes care of untransmitted chunks */
    free(dat);
    event_log(VVERBOSE, "fc_dequeueOldestUnsentChunks(): checking list");
    chunk_list_debug(VVERBOSE, fc->chunk_list);
    return (listlen-1);
}

int fc_readNumberOfUnsentChunks(void)
{
    int queue_len = 0;
    fc_data *fc;
    GList* tmp;
    chunk_data *cdat = NULL;

    fc = (fc_data *) mdi_readFlowControl();
    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return SCTP_MODULE_NOT_FOUND;
    }
    if (fc->chunk_list == NULL) return 0;
    tmp = g_list_first(fc->chunk_list);
    while (tmp) {
        cdat = (chunk_data*)tmp->data; /* deref list data */
        event_logii(VERBOSE, "fc_readNumberOfUnsentChunks(): checking chunk tsn=%u, num_rtx=%u ", cdat->chunk_tsn, cdat->num_of_transmissions);
        if (cdat->num_of_transmissions == 0) queue_len++;
        tmp = g_list_next(tmp);
    }
    event_logi(VERBOSE, "fc_readNumberOfUnsentChunks() returns %u", queue_len);
    return queue_len;
}


/**
 * function returns number of chunks, that are waiting in the transmission queue
 * These have been submitted from the upper layer, but not yet been sent, or
 * retransmitted.
 * @return size of the send queue of the current flowcontrol module
 */
unsigned int fc_readNumberOfQueuedChunks(void)
{
    unsigned int queue_len;
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return 0;
    }
    if (fc->chunk_list != NULL){
        queue_len = fc->list_length;
    }
    else
        queue_len=0;

    event_logi(VERBOSE, "fc_readNumberOfQueuedChunks() returns %u", queue_len);
    return queue_len;
}




/**
 * Function returns cwnd value of a certain path.
 * @param path_id    path index of which we want to know the cwnd
 * @return current cwnd value, else -1
 */
int fc_readCWND(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }

    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return -1;
    }
    return (int)fc->cparams[path_id].cwnd;
}


/**
 * Function returns cwnd2 value of a certain path.
 * @param path_id    path index of which we want to know the cwnd2
 * @return current cwnd2 value, else -1
 */
int fc_readCWND2(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }

    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return -1;
    }
    return (int)fc->cparams[path_id].cwnd2;
}

/**
 * Function returns ssthresh value of a certain path.
 * @param path_id    path index of which we want to know the ssthresh
 * @return current ssthresh value, else -1
 */
int fc_readSsthresh(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return -1;
    }
    return (int)fc->cparams[path_id].ssthresh;
}

/**
 * Function returns mtu value of a certain path.
 * @param path_id    path index of which we want to know the mtu
 * @return current MTU value, else 0
 */
unsigned int fc_readMTU(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return 0;
    }
    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return 0;
    }
    return fc->cparams[path_id].mtu;
}

/**
 * Function returns the partial bytes acked value of a certain path.
 * @param path_id    path index of which we want to know the PBA
 * @return current PBA value, else -1
 */
int fc_readPBA(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return -1;
    }
    return (int)fc->cparams[path_id].partial_bytes_acked;
}

/**
 * Function returns the outstanding byte count value of a certain path.
 * @param path_id    path index of which we want to know the outstanding_bytes_per_address
 * @return current outstanding_bytes_per_address value, else -1
 */
int fc_readOutstandingBytesPerAddress(short path_id)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    if (path_id >= fc->number_of_addresses || path_id < 0) {
        error_logi(ERROR_MAJOR, "Association has only %u addresses !!! ", fc->number_of_addresses);
        return -1;
    }
    return (int)fc->cparams[path_id].outstanding_bytes_per_address;
}

/**
 * Function returns the outstanding byte count value of this association.
 * @return current outstanding_bytes value, else -1
 */
int fc_readOutstandingBytes(void)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    return (int)fc->outstanding_bytes;
}

int fc_get_maxSendQueue(unsigned int * queueLen)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    *queueLen = fc->maxQueueLen;
    return 0;

}

int fc_set_maxSendQueue(unsigned int maxQueueLen)
{
    fc_data *fc;
    fc = (fc_data *) mdi_readFlowControl();

    if (!fc) {
        error_log(ERROR_MAJOR, "flow control instance not set !");
        return -1;
    }
    fc->maxQueueLen = maxQueueLen;
    event_logi(VERBOSE, "fc_set_maxSendQueue(%u)", maxQueueLen);
    return 0;

}
