/*
 *  $Id: chargen_server.c,v 1.5 2003/11/20 13:05:41 tuexen Exp $
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
 * chargen_server.c  - a character generator - main program module
 *
 */


#ifndef WIN32
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sctp_wrapper.h"

#define CHARGEN_PORT                         19
#define MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES    10
#define MAXIMUM_PAYLOAD_LENGTH             8192
#define MAXIMUM_NUMBER_OF_IN_STREAMS         17
#define MAXIMUM_NUMBER_OF_OUT_STREAMS         1
#define BUFFER_LENGTH                      1030
#define SEND_QUEUE_SIZE                     100

static unsigned char  localAddressList[MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES][SCTP_MAX_IP_LEN];
static unsigned short noOfLocalAddresses = 0;
static unsigned char  destinationAddress[SCTP_MAX_IP_LEN];
static unsigned short remotePort              = 9;

static int startAssociation = 0;
static int verbose          = 0;
static int vverbose         = 0;
static int unknownCommand   = 0;
static int sendOOTBAborts   = 1;
static int timeToLive       = SCTP_INFINITE_LIFETIME;

struct ulp_data {
    int ShutdownReceived;
};

void printUsage(void)
{
    printf("Usage:   chargen_server [options]\n");
    printf("options:\n");
    printf("-d destination_addr     establish a association with the specified address\n");   
    printf("-i       ignore OOTB packets\n");
    printf("-n number    number of packets after which to stop and shutdown\n");   
    printf("-r port  connect to this remote port\n");
    printf("-s       source address\n");
    printf("-t       time to live in ms\n");
    printf("-v       verbose mode\n");
    printf("-V       very verbose mode\n");   
}

void getArgs(int argc, char **argv)
{
    int i;
    char *opt;
    
    for(i=1; i < argc ;i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
				case 'h':
					printUsage();
					exit(0);
				case 'd':
					if (i+1 >= argc) {
                        printUsage();
				        exit(0);
				    }
				    opt = argv[++i];
					if (strlen(opt) < SCTP_MAX_IP_LEN) {
						strcpy((char *)destinationAddress, opt);
						startAssociation = 1;
					}
					break;
				case 'r':
					if (i+1 >= argc) {
                        printUsage();
				        exit(0);
				    }
				    opt = argv[++i];
					remotePort =  atoi(opt);
					break;
				case 's':
					if (i+1 >= argc) {
                        printUsage();
				        exit(0);
				    }
				    opt = argv[++i];
					if ((noOfLocalAddresses < MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES) &&
						(strlen(opt) < SCTP_MAX_IP_LEN  )) {
						strcpy((char *)localAddressList[noOfLocalAddresses], opt);
						noOfLocalAddresses++;
					}; 
					break;
				case 'i':
					sendOOTBAborts = 0;
					break;
				case 't':
					if (i+1 >= argc) {
                        printUsage();
				        exit(0);
				    }
				    opt = argv[++i];
					timeToLive = atoi(opt);
					break;
				case 'v':
					verbose = 1;
					break;
				case 'V':
					verbose = 1;
					vverbose = 1;
					break;
				default:
					unknownCommand = 1;
					break;
			}
		}
		else
			unknownCommand = 1;
	}
}

void checkArgs(void)
{
    int abortProgram;
    int printUsageInfo;
    
    abortProgram = 0;
    printUsageInfo = 0;
    
    if (noOfLocalAddresses == 0) {
#ifdef HAVE_IPV6
        strcpy((char *)localAddressList[noOfLocalAddresses], "::0");
#else
        strcpy((char *)localAddressList[noOfLocalAddresses], "0.0.0.0");
#endif
        noOfLocalAddresses++;
    }
    if (unknownCommand ==1) {
         printf("Error:   Unkown options in command.\n");
         printUsageInfo = 1;
         abortProgram = 1;
    }
    
    if (printUsageInfo == 1)
        printUsage();
    if (abortProgram == 1)
        exit(-1);
}

void dataArriveNotif(unsigned int assocID, unsigned int streamID, unsigned int len,
                     unsigned short streamSN,unsigned int TSN, unsigned int protoID,
                     unsigned int unordered, void* ulpDataPtr)
{
    unsigned char chunk[MAXIMUM_PAYLOAD_LENGTH];
    unsigned int length;
    unsigned short ssn;
    unsigned int tsn;

    if (vverbose) {  
      fprintf(stdout, "%-8x: Data arrived (%u bytes on stream %u, %s)\n",
                      assocID, len, streamID, (unordered==SCTP_ORDERED_DELIVERY)?"ordered":"unordered");
      fflush(stdout);
    }
    /* read it */
    length = MAXIMUM_PAYLOAD_LENGTH;
    SCTP_receive(assocID, streamID, chunk, &length, &ssn, &tsn, SCTP_MSG_DEFAULT);
}

void sendFailureNotif(unsigned int assocID,
                      unsigned char *unsent_data, unsigned int dataLength, unsigned int *context, void* ulpDataPtr)
{
  if (verbose) {  
    fprintf(stdout, "%-8x: Send failure\n", assocID);
    fflush(stdout);
  }
}

void networkStatusChangeNotif(unsigned int assocID, short destAddrIndex, unsigned short newState, void* ulpDataPtr)
{
    SCTP_AssociationStatus assocStatus;
    SCTP_PathStatus pathStatus;
    unsigned short pathID;
    
    if (verbose) {  
        fprintf(stdout, "%-8x: Network status change: path %u is now %s\n", 
                        assocID, destAddrIndex, ((newState == SCTP_PATH_OK) ? "ACTIVE" : "INACTIVE"));
        fflush(stdout);
    }
    
    /* if the primary path has become inactive */
    if ((newState == SCTP_PATH_UNREACHABLE) &&
        (destAddrIndex == SCTP_getPrimary(assocID))) {
        
        /* select a new one */ 
        SCTP_getAssocStatus(assocID, &assocStatus);
        for (pathID=0; pathID < assocStatus.numberOfAddresses; pathID++){
            SCTP_getPathStatus(assocID, pathID, &pathStatus);
            if (pathStatus.state == SCTP_PATH_OK)
                break;
        }
        
        /* and use it */
        if (pathID < assocStatus.numberOfAddresses) {
            SCTP_setPrimary(assocID, pathID);
            if (verbose) {
                fprintf(stdout, "%-8x: Set new primary path: path %u \n",
                        assocID, pathID);
                fflush(stdout);
            }
        }
    }
}

void* communicationUpNotif(unsigned int assocID, int status,
                           unsigned int noOfDestinations,
                           unsigned short noOfInStreams, unsigned short noOfOutStreams,
                           int associationSupportsPRSCTP, void* dummy)
{	
    unsigned int length;
    unsigned char buffer[BUFFER_LENGTH];
    struct ulp_data *ulpDataPtr;
    
    if (verbose) {  
        fprintf(stdout, "%-8x: Communication up (%u paths)\n", assocID, noOfDestinations);
        fflush(stdout);
    }
    
    length = 1 + (rand() % (BUFFER_LENGTH - 1));
    memset((void *)buffer, 'A', length);
    buffer[length-1] = '\n';

    while(SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                    SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                    SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS) {
        if (vverbose) {
            fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
            fflush(stdout);
        }
        length = 1 + (rand() % 1024);
        memset((void *)buffer, 'A', length);
        buffer[length-1] = '\n';
    }

    ulpDataPtr = malloc(sizeof(struct ulp_data));
    ulpDataPtr->ShutdownReceived = 0;
    return (void *)ulpDataPtr;
}

void communicationLostNotif(unsigned int assocID, unsigned short status, void* ulpDataPtr)
{	
    if (verbose) {
        fprintf(stdout, "%-8x: Communication lost (status %u)\n", assocID, status);
        fflush(stdout);
    }
    free((struct ulp_data *)ulpDataPtr);
    SCTP_deleteAssociation(assocID);
}

void communicationErrorNotif(unsigned int assocID, unsigned short status, void* dummy)
{
    if (verbose) {  
        fprintf(stdout, "%-8x: Communication error (status %u)\n", assocID, status);
        fflush(stdout);
    }
}

void restartNotif(unsigned int assocID, void* ulpDataPtr)
{    
    unsigned int length;
    unsigned char buffer[BUFFER_LENGTH];

    if (verbose) {  
        fprintf(stdout, "%-8x: Restart\n", assocID);
        fflush(stdout);
    }
    
    length = 1 + (rand() % (BUFFER_LENGTH - 1));
    memset((void *)buffer, 'A', length);
    buffer[length-1] = '\n';
    
    while((!(((struct ulp_data *)ulpDataPtr)->ShutdownReceived)) &&
          (SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                     SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                     SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS)) {
      if (vverbose) {
          fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
          fflush(stdout);
      }
      length = 1 + (rand() % 1024);
      memset(buffer, 'A', length);
      buffer[length-1] = '\n';
    }
}

void shutdownCompleteNotif(unsigned int assocID, void* ulpDataPtr)
{

    if (verbose) {  
        fprintf(stdout, "%-8x: Shutdown complete\n", assocID);
        fflush(stdout);
    }

    free((struct ulp_data *) ulpDataPtr);
    SCTP_deleteAssociation(assocID);
}

void queueStatusChangeNotif(unsigned int assocID, int queueType, int queueID, int queueLength, void* ulpDataPtr)
{	
    unsigned int length;
    unsigned char buffer[BUFFER_LENGTH];

    if (vverbose) {
        fprintf(stdout, "%-8x: Queue status change notification: Type %d, ID %d, Length %d\n",
                        assocID, queueType, queueID, queueLength);
        fflush(stdout);
    }
    
    /* if (queueType == SCTP_SEND_QUEUE) { */
    if (queueType == SCTP_SEND_QUEUE && queueLength <= SEND_QUEUE_SIZE) { 
      length = 1 + (rand() % (BUFFER_LENGTH - 1));
      memset((void *)buffer, 'A', length);
      buffer[length-1] = '\n';
      
      while((!(((struct ulp_data *)ulpDataPtr)->ShutdownReceived)) &&
            (SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                       SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                       SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS)) {
        if (vverbose) {
            fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
            fflush(stdout);
        }

        length = 1 + (rand() % 1024);
        memset((void *)buffer, 'A', length);
        buffer[length-1] = '\n';
      }
    }
}

void shutdownReceivedNotif(unsigned int assocID, void* ulpDataPtr)
{
    if (verbose) {  
        fprintf(stdout, "%-8x: Shutdown received\n", assocID);
        fflush(stdout);
    }
    ((struct ulp_data *)ulpDataPtr)->ShutdownReceived = 1;
}


int main(int argc, char **argv)
{
    SCTP_ulpCallbacks chargenUlp;
    SCTP_LibraryParameters params;
    SCTP_InstanceParameters instanceParameters;
    short sctpInstance;
    
    /* initialize the discard_ulp variable */
    chargenUlp.dataArriveNotif           = &dataArriveNotif;
    chargenUlp.sendFailureNotif          = &sendFailureNotif;
    chargenUlp.networkStatusChangeNotif  = &networkStatusChangeNotif;
    chargenUlp.communicationUpNotif      = &communicationUpNotif;
    chargenUlp.communicationLostNotif    = &communicationLostNotif;
    chargenUlp.communicationErrorNotif   = &communicationErrorNotif;
    chargenUlp.restartNotif              = &restartNotif;
    chargenUlp.shutdownCompleteNotif     = &shutdownCompleteNotif;
    chargenUlp.queueStatusChangeNotif    = &queueStatusChangeNotif;
    chargenUlp.peerShutdownReceivedNotif = &shutdownReceivedNotif;

    /* handle all command line options */
    getArgs(argc, argv);
    checkArgs();

    SCTP_initLibrary();
    SCTP_getLibraryParameters(&params);
    params.sendOotbAborts    = sendOOTBAborts;
    params.checksumAlgorithm = SCTP_CHECKSUM_ALGORITHM_CRC32C;
    SCTP_setLibraryParameters(&params);

    /* set up the "server" */
    sctpInstance = SCTP_registerInstance(CHARGEN_PORT,
                                         MAXIMUM_NUMBER_OF_IN_STREAMS, MAXIMUM_NUMBER_OF_OUT_STREAMS,
                                         noOfLocalAddresses, localAddressList,
                                         chargenUlp);

    SCTP_getAssocDefaults(sctpInstance, &instanceParameters);
    instanceParameters.maxSendQueue = SEND_QUEUE_SIZE;
    SCTP_setAssocDefaults(sctpInstance, &instanceParameters);

    if (startAssociation) {
        SCTP_associate(sctpInstance, MAXIMUM_NUMBER_OF_OUT_STREAMS, destinationAddress, remotePort, NULL);
    }
    /* run the event handler forever */
    while (1) {
        SCTP_eventLoop();
    }
    
    /* this will never be reached */
    exit(0);
}




