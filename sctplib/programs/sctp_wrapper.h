/*
 *  $Id: sctp_wrapper.h,v 1.4 2004/08/11 12:01:02 tuexen Exp $
 *
 * SCTP implementation according to RFC 2960.
 * Copyright (C) 2000 by Siemens AG, Munich, Germany.
 *
 * Realized in co-operation between Siemens AG
 * and University of Essen, Institute of Computer Networking Technology.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * There are two mailinglists available at http://www.sctp.de which should be
 * used for any discussion related to this implementation.
 *
 * Contact: discussion@sctp.de
 *          tuexen@fh-muenster.de
 *          ajung@exp-math.uni-essen.de
 *
 * wrapper for SCTPLIB functions. Outputs appropriate error messages
 *
 */
#include "../sctp/sctp.h"



#ifdef __cplusplus
extern "C" {
#endif


int 
SCTP_getLibraryParameters(SCTP_LibraryParameters *params);

int 
SCTP_setLibraryParameters(SCTP_LibraryParameters *params);

int
SCTP_initLibrary(void);

short
SCTP_registerInstance(unsigned short port,
                      unsigned short noOfInStreams,
                      unsigned short noOfOutStreams,
                      unsigned int noOfLocalAddresses,
                      unsigned char localAddressList[][SCTP_MAX_IP_LEN],
                      SCTP_ulpCallbacks ULPcallbackFunctions);

int
SCTP_unregisterInstance(unsigned short instance_name);

int
SCTP_deleteAssociation(unsigned int associationID);

int 
SCTP_send(unsigned int associationID, unsigned short streamID,
          unsigned char *buffer, unsigned int length, unsigned int protocolId, short path_id,
          void*  context, 
          unsigned int lifetime,
          int unorderedDelivery, 
          int dontBundle);
int
SCTP_receive(unsigned int associationID, unsigned short streamID, unsigned char *buffer,
			unsigned int *length, unsigned short* streamSN, unsigned int * tsn, unsigned int flags);

int
SCTP_shutdown(unsigned int associationID);

int
SCTP_abort(unsigned int associationID);

short 
SCTP_setPrimary(unsigned int associationID, short path_id);

short
SCTP_getPrimary(unsigned int associationID);

int
SCTP_receiveUnacked(unsigned int associationID, unsigned char *buffer, unsigned int *length, unsigned int* tsn,
                    unsigned short *streamID, unsigned short *streamSN,unsigned int* protocolId);
int
SCTP_receiveUnsent(unsigned int associationID, unsigned char *buffer, unsigned int *length,unsigned int* tsn,
                   unsigned short *streamID, unsigned short *streamSN,unsigned int* protocolId);

int
SCTP_eventLoop(void);

int 
SCTP_getPathStatus(unsigned int associationID, short path_id, SCTP_PathStatus* status);

int
SCTP_getAssocStatus(unsigned int associationID, SCTP_AssociationStatus* status);

unsigned int
SCTP_associate(unsigned short SCTP_InstanceName,
               unsigned short noOfOutStreams,
               unsigned char destinationAddress[],
               unsigned short destinationPort,
               void* ulp_data);
int
SCTP_registerUserCallback(int fd, sctp_userCallback sdf, void* userData);

int
SCTP_getAssocDefaults(unsigned short SCTP_InstanceName, SCTP_InstanceParameters* params);

int
SCTP_setAssocDefaults(unsigned short SCTP_InstanceName, SCTP_InstanceParameters* params);

unsigned int
SCTP_startTimer(unsigned int milliseconds, sctp_timerCallback timer_cb,
                void *param1, void *param2);


#ifdef __cplusplus
}
#endif

