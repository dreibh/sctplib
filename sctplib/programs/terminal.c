/*
 *  $Id: terminal.c,v 1.3 2003/07/01 13:58:26 ajung Exp $
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
 * terminal.c  - main program module
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sctp_wrapper.h"


#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>         /* for atoi() under Linux */

#define ECHO_PORT                              7
#define MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES     10
#define MAXIMUM_NUMBER_OF_IN_STREAMS          10
#define MAXIMUM_NUMBER_OF_OUT_STREAMS         10
#define min(x,y)                              (x)<(y)?(x):(y)

#if defined SCTP_MAXIMUM_DATA_LENGTH
    #undef SCTP_MAXIMUM_DATA_LENGTH
#endif

#define SCTP_MAXIMUM_DATA_LENGTH    4500

static unsigned char localAddressList[MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES][SCTP_MAX_IP_LEN];
static unsigned char destinationAddress[SCTP_MAX_IP_LEN];

static unsigned short noOfLocalAddresses = 0;

static unsigned short remotePort = ECHO_PORT;
static unsigned short localPort  = 1000;
static unsigned char  tosByte    = 0x10;  /* IPTOS_LOWDELAY */
static unsigned int associationID;
static unsigned short sctpInstance;
static int useAbort = 0;
static int sendOOTBAborts = 1;
static unsigned int myRwnd = 0;
static int myRwndSpecified = 0;
static int timeToLive     = SCTP_INFINITE_LIFETIME;
static short numberOutStreams = 0;
static short currentStream = 0;

static int rotateStreams = 0;
static int verbose  = 0;
static int vverbose = 0;
static int unknownCommand = 0;
static int hasDestinationAddress = 0;

void printUsage(void)
{
    printf("Usage:   terminal [options] -d destination_addr ...\n");
    printf("options:\n");
    printf("-a       use abort\n");
    printf("-c       use all streams when sending (round-robin)\n");
    printf("-l       local port\n");   
    printf("-q       type of service\n");
    printf("-r       remote port (default echoport = %u)\n", ECHO_PORT);
    printf("-s       source address\n");   
    printf("-i       ignore OOTB packets\n");
    printf("-t       time to live in ms\n");
    printf("-v       verbose mode\n");
    printf("-V       very verbose mode\n");
    printf("-w       receiver Window\n");   
}

void getArgs(int argc, char **argv)
{
    int c;
    extern char *optarg;
    extern int optind;

    while ((c = getopt(argc, argv, "acd:l:r:s:q:t:ivVw:")) != -1)
    {
        switch (c) {
        case 'a':
            useAbort = 1;
            break;
        case 'c':
            rotateStreams = 1;
            break;
        case 'd':
            if (strlen(optarg) < SCTP_MAX_IP_LEN) {
                strcpy(destinationAddress, optarg);
            }
            hasDestinationAddress = 1;
            break;
        case 'l':
            localPort = atoi(optarg);
            break;
        case 'q':
            tosByte = (unsigned char) atoi(optarg);
            break;
        case 'r':
            remotePort = atoi(optarg);
            break;
        case 's':
            if ((noOfLocalAddresses < MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES) &&
                (strlen(optarg) < SCTP_MAX_IP_LEN  )) {
                strcpy(localAddressList[noOfLocalAddresses], optarg);
                noOfLocalAddresses++;
            }
            break;
        case 't':
            timeToLive = atoi(optarg);
            break;
        case 'i':
            sendOOTBAborts = 0;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'V':
            verbose = 1;
            vverbose = 1;
            break;
        case 'w':
            myRwnd = atoi(optarg);
            myRwndSpecified = 1;
            break;
        default:
            unknownCommand = 1;
            break;
        }
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
        strcpy(localAddressList[noOfLocalAddresses], "::0");
#else
        strcpy(localAddressList[noOfLocalAddresses], "0.0.0.0");
#endif
        noOfLocalAddresses++;
    }
    if (hasDestinationAddress==0) {
        printf("Error:   An destination address must be specified.\n");
        abortProgram = 1;
        printUsageInfo = 1;
    }
    if (unknownCommand ==1) {
         printf("Error:   Unkown options in command.\n");
         printUsageInfo = 1;
    }
    
    if (printUsageInfo == 1)
        printUsage();
    if (abortProgram == 1)
        exit(-1);
}

void dataArriveNotif(unsigned int assocID, unsigned int streamID, unsigned int len,
                     unsigned short streamSN, unsigned int TSN, unsigned int protoID,
                     unsigned int unordered, void* ulpDataPtr)
{
    char chunk[SCTP_MAXIMUM_DATA_LENGTH + 1];
    int length;
    unsigned short ssn;
    unsigned int the_tsn;
 
    if (vverbose) {  
      fprintf(stdout, "%-8x: Data arrived (%u bytes on stream %u, %s)\n",
                      assocID, len, streamID, (unordered==SCTP_ORDERED_DELIVERY)?"ordered":"unordered");
      fflush(stdout);
    }
    /* read it */
    length = SCTP_MAXIMUM_DATA_LENGTH;
    SCTP_receive(assocID, streamID, chunk, &length,&ssn, &the_tsn, SCTP_MSG_DEFAULT);
    chunk[length]=0;
    fprintf(stdout, "%s", chunk);
    fflush(stdout);
}

void sendFailureNotif(unsigned int assocID,
                      unsigned char *unsent_data, unsigned int dataLength, unsigned int *context, void* dummy)
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
        }
    }
}

void* communicationUpNotif(unsigned int assocID, int status,
                           unsigned int noOfDestinations,
                           unsigned short noOfInStreams, unsigned short noOfOutStreams,
                           int associationSupportsPRSCTP, void* dummy)
{	
    if (verbose) {  
        fprintf(stdout, "%-8x: Communication up (%u paths, %u In-Streams, %u Out-Streams)\n", assocID, noOfDestinations, noOfInStreams, noOfOutStreams);
        fflush(stdout);
    }
    numberOutStreams = noOfOutStreams;
    return NULL;
}

void communicationLostNotif(unsigned int assocID, unsigned short status, void* ulpDataPtr)
{	
    unsigned char buffer[SCTP_MAXIMUM_DATA_LENGTH];
    unsigned int bufferLength;
    unsigned short streamID, streamSN;
    unsigned int protoID;
    unsigned int tsn;

    if (verbose) {
        fprintf(stdout, "%-8x: Communication lost (status %u)\n", assocID, status);
        fflush(stdout);
    }
    
    /* retrieve data */
    bufferLength = sizeof(buffer);
    while (SCTP_receiveUnsent(assocID, buffer, &bufferLength, &tsn, &streamID, &streamSN, &protoID) >= 0){
        if (vverbose) {
            fprintf(stdout, "%-8x: Unsent data (%u bytes) retrieved (TSN = %u, SID = %u, SSN = %u, PPI = %u): %s", 
                            assocID, bufferLength, tsn, streamID, streamSN, protoID, buffer);
            fflush(stdout);
        }
        bufferLength = sizeof(buffer);
    }
    
    bufferLength = sizeof(buffer);
    while (SCTP_receiveUnacked(assocID, buffer, &bufferLength, &tsn, &streamID, &streamSN, &protoID) >= 0){
        if (vverbose) {
            fprintf(stdout, "%-8x: Unacked data (%u bytes) retrieved (TSN = %u, SID = %u, SSN = %u, PPI = %u): %s", 
                            assocID, bufferLength, tsn, streamID, streamSN, protoID, buffer);
            fflush(stdout);
        }
        bufferLength = sizeof(buffer);
    }
                      
    /* delete the association, instace and terminate */
    SCTP_deleteAssociation(assocID);
    SCTP_unregisterInstance(sctpInstance);
    exit(0);
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
    if (verbose) {  
        fprintf(stdout, "%-8x: Restart\n", assocID);
        fflush(stdout);
    }
}

void shutdownCompleteNotif(unsigned int assocID, void* ulpDataPtr)
{
    if (verbose) {  
        fprintf(stdout, "%-8x: Shutdown complete\n", assocID);
        fflush(stdout);
    }
    
    /* delete the association, instance and terminate */
    SCTP_deleteAssociation(assocID);
    SCTP_unregisterInstance(sctpInstance);
    exit(0); 
}

void shutdownReceivedNotif(unsigned int assocID, void* ulpDataPtr)
{
    if (verbose) {  
        fprintf(stdout, "%-8x: Shutdown received\n", assocID);
        fflush(stdout);
    }
}

void stdinCallback(int fd, short int revents, short int* gotEvents,  void* dummy)
{
    unsigned char readBuffer[SCTP_MAXIMUM_DATA_LENGTH];

    memset(readBuffer, (int)currentStream, sizeof(readBuffer));
    
    if (fgets(readBuffer, sizeof(readBuffer), stdin)==NULL) {
        if (useAbort) {
            SCTP_abort(associationID);
        } else {
            SCTP_shutdown(associationID);
        }
    } else {
        SCTP_send(associationID,
                  currentStream,
                  readBuffer,  strlen(readBuffer) /*sizeof(readBuffer)*/,
                  SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                  SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, 
                  timeToLive, SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED);
    }
    if (rotateStreams) currentStream = (currentStream + 1)%numberOutStreams;
}

int main(int argc, char **argv)
{
    SCTP_ulpCallbacks terminalUlp;
    SCTP_InstanceParameters instanceParameters;
    SCTP_LibraryParameters params;

    /* initialize the terminal_ulp variable */
    terminalUlp.dataArriveNotif           = &dataArriveNotif;
    terminalUlp.sendFailureNotif          = &sendFailureNotif;
    terminalUlp.networkStatusChangeNotif  = &networkStatusChangeNotif;
    terminalUlp.communicationUpNotif      = &communicationUpNotif;
    terminalUlp.communicationLostNotif    = &communicationLostNotif;
    terminalUlp.communicationErrorNotif   = &communicationErrorNotif;
    terminalUlp.restartNotif              = &restartNotif;
    terminalUlp.shutdownCompleteNotif     = &shutdownCompleteNotif;
    terminalUlp.peerShutdownReceivedNotif = &shutdownReceivedNotif;

    /* handle all command line options */
    getArgs(argc, argv);
    checkArgs();
    
    SCTP_initLibrary();
    SCTP_getLibraryParameters(&params);
    params.sendOotbAborts    = sendOOTBAborts;
    params.supportPRSCTP     = 1;
    params.checksumAlgorithm = SCTP_CHECKSUM_ALGORITHM_CRC32C;
    SCTP_setLibraryParameters(&params);

    sctpInstance=SCTP_registerInstance(localPort,
                                       MAXIMUM_NUMBER_OF_IN_STREAMS,  MAXIMUM_NUMBER_OF_OUT_STREAMS,
                                       noOfLocalAddresses, localAddressList,
                                       terminalUlp);

    /* set the TOS byte */
    SCTP_getAssocDefaults(sctpInstance, &instanceParameters);
    instanceParameters.ipTos               = tosByte;
    if (myRwndSpecified)
      instanceParameters.myRwnd = myRwnd;
    SCTP_setAssocDefaults(sctpInstance, &instanceParameters);

    SCTP_registerUserCallback(fileno(stdin), &stdinCallback, NULL);

    associationID=SCTP_associate(sctpInstance, MAXIMUM_NUMBER_OF_OUT_STREAMS, destinationAddress, remotePort, NULL);
    
    /* run the event handler forever */
    while (1){
        SCTP_eventLoop();
    }

    /* this will never be reached */
    exit(0);
}


