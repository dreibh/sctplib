/*
 *  $Id: sctp_asconf.c,v 1.1 2003/05/16 13:47:49 ajung Exp $
 *
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
 * There are two mailinglists available at www.sctp.de which should be used for
 * any discussion related to this implementation.
 *
 * Contact: discussion@sctp.de
 *          Michael.Tuexen@icn.siemens.de
 *          ajung@exp-math.uni-essen.de
 *
 * Purpose: This file implements the handling of the per address/per stream
 *          configuration changes that are defined in
 *          draft-ietf-tsvwg-stewart-addip-02.txt
 *
 */

/*
 * general initialization function - not sure if we actually need this
 */

#include <glib.h>

#include "sctp_asconf.h"
#include "sctp.h"
#include "distribution.h"


typedef struct _asc_data {
    gboolean        peer_supports_addip;
    gboolean        peer_supports_delip;
    gboolean        peer_supports_setPrimary;
    gboolean        peer_supports_stream_byte_limit;
    gboolean        peer_supports_stream_msg_limit;
    gboolean        peer_supports_assoc_msg_limit;

    gboolean        i_support_addip;
    gboolean        i_support_delip;
    gboolean        i_support_setPrimary;
    gboolean        i_support_stream_byte_limit;
    gboolean        i_support_stream_msg_limit;
    gboolean        i_support_assoc_msg_limit;

    guint32         my_serial_number;
    guint32         peer_serial_number;
    guint32         currentCorrelationId;
    gboolean        asconf_outstanding;
    TimerID         asconf_timer;
    GList*          queued_asconf_requests;
    SCTP_asconf*        lastRequestSent;
    SCTP_asconf_ack*    lastReplySent;
}asc_data;



typedef struct ASCONF_ADDDELSETPARAM
{
    guint32         correlationId;
    guint16         pType;
    guint16         pLength;
    SCTP_ip_address address;
}
asconf_AddDelSetParam;



int asc_init(void)
{
 return 0;
}

/*
 * asc_new: Create a new AS_CONF instance and returns a pointer to its data.
 */
void *asc_new(guint32 initial_serial, guint32 peer_initial_serial)
{
    /* Alloc new bundling_instance data struct */
    asc_data *ptr;

    ptr = malloc(sizeof(asc_data));
    if (!ptr) {
        error_log(ERROR_FATAL, "Malloc failed in asc_new()");
        return NULL;
    }
    ptr->peer_supports_addip = TRUE;
    ptr->peer_supports_delip = TRUE;
    ptr->peer_supports_setPrimary = TRUE;
    ptr->peer_supports_stream_byte_limit = FALSE;
    ptr->peer_supports_stream_msg_limit = FALSE;
    ptr->peer_supports_assoc_msg_limit = FALSE;

    ptr->i_support_addip = TRUE;
    ptr->i_support_delip = TRUE;
    ptr->i_support_setPrimary = TRUE;
    ptr->i_support_stream_byte_limit = FALSE;
    ptr->i_support_stream_msg_limit = FALSE;
    ptr->i_support_assoc_msg_limit = FALSE;

    ptr->my_serial_number = initial_serial;
    ptr->peer_serial_number = peer_initial_serial;
    ptr->currentCorrelationId = 1;

    ptr->asconf_outstanding = FALSE;
    ptr->asconf_timer = 0;

    ptr->queued_asconf_requests = NULL;
    ptr->lastRequestSent = NULL;
    ptr->lastReplySent = NULL;

    return ptr;

}

/*
 * asc_delete: Deletes a AS_CONF instance
 *
 * Params: Pointer/handle which was returned by asc_new()
 */
int asc_delete(void *asConfPtr)
{
    if (asConfPtr == NULL) return -1;
    free(asConfPtr);
    return 0;
}

/*
 *  asc_recv_asconf_chunk gets a pointer to an AS_CONF chunk and decodes it
 *  accordingly....
 *  @return  error code, 0 for success, less than one for error
 */
int asc_recv_asconf_chunk(SCTP_simple_chunk * asc_chunk)
{
    asc_data *ascd;

    ascd = (asc_data *) mdi_readASCONF();
    if (!ascd) {
        error_log(ERROR_MAJOR, "ASCONF instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }
    /*    check if we take action        */
    /*    send acknowledgement or drop   */
    return 0;
}


/*
 *  asc_recv_asconf_ack gets a pointer to an AS_CONF_ACK chunk and decodes it
 *  accordingly....
 *  @return  error code, 0 for success, less than one for error
 */
int asc_recv_asconf_ack(SCTP_simple_chunk * asc_ack)
{
    asc_data *ascd;

    ascd = (asc_data *) mdi_readASCONF();
    if (!ascd) {
        error_log(ERROR_MAJOR, "ASCONF instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }
    /* check if we can send another one now, that one has been acked */
    return 0;
}

unsigned char* asc_buildParam(unsigned int corrId, unsigned short type, union sockunion* theIP)
{
    asconf_AddDelSetParam* param;
    int addressLen;

    param = malloc(sizeof(asconf_AddDelSetParam));
    if (param == NULL) return NULL;

    param->correlationId = corrId;
    param->pType = htons(type);
    switch (sockunion_family(theIP)) {
    case AF_INET:
        addressLen = 8;
        param->address.vlparam_header.param_type = htons(VLPARAM_IPV4_ADDRESS);
        param->address.vlparam_header.param_length = htons(8);
        param->address.dest_addr.sctp_ipv4 = sock2ip(theIP);
        break;
#ifdef HAVE_IPV6
    case AF_INET6:
        addressLen = 20;
        param->address.vlparam_header.param_type = htons(VLPARAM_IPV6_ADDRESS);
        param->address.vlparam_header.param_length = htons(20);
        memcpy(param->address.dest_addr.sctp_ipv6, &(sock2ip6(theIP)), sizeof(struct in6_addr));
        break;
#endif
    default:
        error_logi(ERROR_MAJOR, "Address family %d not supported", sockunion_family(theIP));
        free(param);
        return NULL;
        break;
    }
    param->pLength = htons(addressLen+4);
    return ((unsigned char*)param);
}


int asc_sendRequest(asc_data *ascd)
{
    /* we have data queued, now build ASCONF REQUEST chunk */

    /* add all Parameters we have around (up to a maximum of 25 requests) */

    /* send it out, and store the pointer to the chunk for retransmission */

    /* start timer with primary address timeout */
    return -1;
}



int asc_addIP(union sockunion* addedIP, unsigned int* corrId)
{
    asc_data *ascd;
    int result = SCTP_SUCCESS;
    asconf_AddDelSetParam* param;

    ascd = (asc_data *) mdi_readASCONF();
    if (!ascd) {
        error_log(ERROR_MAJOR, "ASCONF instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }
    if (ascd->peer_supports_addip == FALSE) return SCTP_NOT_SUPPORTED;

    param = (asconf_AddDelSetParam*)asc_buildParam(ascd->currentCorrelationId, VLPARAM_ADDIP, addedIP);
    *corrId = ascd->currentCorrelationId++;

    /* queue it */
    ascd->queued_asconf_requests = g_list_append(ascd->queued_asconf_requests,
                                                 param);

    /* if none oustanding then send it */
    if (ascd->asconf_outstanding == FALSE)  result = asc_sendRequest(ascd);

    return result;
}

int asc_deleteIP(union sockunion* deletedIP, unsigned int* corrId)
{
    asc_data *ascd;
    int result = SCTP_SUCCESS;
    asconf_AddDelSetParam* param;

    ascd = (asc_data *) mdi_readASCONF();
    if (!ascd) {
        error_log(ERROR_MAJOR, "ASCONF instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }
    if (ascd->peer_supports_delip == FALSE) return SCTP_NOT_SUPPORTED;

    param = (asconf_AddDelSetParam*)asc_buildParam(ascd->currentCorrelationId, VLPARAM_DELIP, deletedIP);
    *corrId = ascd->currentCorrelationId++;

    /* queue it */
    ascd->queued_asconf_requests = g_list_append(ascd->queued_asconf_requests,
                                                 param);

    /* if none oustanding then send it */
    if (ascd->asconf_outstanding == FALSE)  result = asc_sendRequest(ascd);

    return result;
}

int asc_setRemotePrimary(union sockunion* theIP)
{
    asc_data *ascd;
    int result = SCTP_SUCCESS;
    asconf_AddDelSetParam* param;

    ascd = (asc_data *) mdi_readASCONF();
    if (!ascd) {
        error_log(ERROR_MAJOR, "ASCONF instance not set !");
        return (SCTP_MODULE_NOT_FOUND);
    }
    if (ascd->peer_supports_setPrimary == FALSE) return SCTP_NOT_SUPPORTED;

    param = (asconf_AddDelSetParam*)asc_buildParam(ascd->currentCorrelationId, VLPARAM_SET_PRIMARY, theIP);
    ascd->currentCorrelationId++;

    /* queue it */
    ascd->queued_asconf_requests = g_list_append(ascd->queued_asconf_requests,
                                                 param);

    /* if none oustanding then send it */
    if (ascd->asconf_outstanding == FALSE)  result = asc_sendRequest(ascd);

    return result;
}

