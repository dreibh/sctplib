/*
 *  $Id: sctp_asconf.h,v 1.1 2003/05/16 13:47:49 ajung Exp $
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
 * Purpose: This header-file defines the interface to the functions used for
 *          the per stream/per address configuration chunk handling
 *
 */


#ifndef ASCONF_H
#define ASCONF_H


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif



#include "globals.h"            /* for chunk struct definition etc. */

/*
 * general initialization function - not sure if we actually need this
 */
int asc_init(void);

/*
 * asc_new: Create a new AS_CONF instance and returns a pointer to its data.
 */
void *asc_new(guint32 initial_serial, guint32 peer_initial_serial);

/*
 * asc_delete: Deletes a AS_CONF instance
 *
 * Params: Pointer/handle which was returned by asc_new()
 * returns 0 if okay, else -11 for error
 *
 */
int asc_delete(void *asConfPtr);

/*
 *  asc_recv_chunk gets a pointer to an AS_CONF chunk and decodes it
 *  accordingly....
 *  @return  error code, 0 for success, less than one for error
 */
int asc_recv_asconf_chunk(SCTP_simple_chunk * asc_chunk);

/*
 *  asc_recv_asconf_ack gets a pointer to an AS_CONF_ACK chunk and decodes it
 *  accordingly....
 *  @return  error code, 0 for success, less than one for error
 */
int asc_recv_asconf_ack(SCTP_simple_chunk * asc_ack);


int asc_addIP(union sockunion* addedIP, unsigned int* corrId);

int asc_deleteIP(union sockunion* deletedIP, unsigned int* corrId);

int asc_setRemotePrimary(union sockunion* theIP);


#endif
