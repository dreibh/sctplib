/*
 *  $Id: chargen_server.c,v 1.1 2003/05/16 13:47:50 ajung Exp $
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
 *          Michael.Tuexen@icn.siemens.de
 *          ajung@exp-math.uni-essen.de
 *
 * chargen_server.c  - a character generator - main program module
 *
 */



#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sctp_wrapper.h>

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

static unsigned int numberOfPacketsToSend  = 0; /* i.e. unlimited */
static unsigned int numberOfPacketsSent    = 0;
static unsigned long int numberOfBytesSent      = 0;

static struct timeval start_time, end_time, diff_time;

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
    int c;
    extern char *optarg;
    extern int optind;

    while ((c = getopt(argc, argv, "hr:s:d:n:it:vV")) != -1)
    {
        switch (c) {
        case 'h':
            printUsage();
            exit(0);
        case 'd':
            if (strlen(optarg) < SCTP_MAX_IP_LEN) {
                strcpy(destinationAddress, optarg);
                startAssociation = 1;
            }
            break;
        case 'r':
            remotePort =  atoi(optarg);
            break;
        case 's':
            if ((noOfLocalAddresses < MAXIMUM_NUMBER_OF_LOCAL_ADDRESSES) &&
                (strlen(optarg) < SCTP_MAX_IP_LEN  )) {
                strcpy(localAddressList[noOfLocalAddresses], optarg);
                noOfLocalAddresses++;
            }; 
            break;
        case 'i':
            sendOOTBAborts = 0;
            break;
        case 'n':
            numberOfPacketsToSend =  atoi(optarg);
            break;
        case 't':
            timeToLive = atoi(optarg);
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
    int length;
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
    int length;
    char buffer[BUFFER_LENGTH];
    struct ulp_data *ulpDataPtr;
    
    if (verbose) {  
        fprintf(stdout, "%-8x: Communication up (%u paths)\n", assocID, noOfDestinations);
        fflush(stdout);
    }
    
    length = 1 + (random() % 1024);
    memset(buffer, 'A', length);
    buffer[length-1] = '\n';

    gettimeofday(&start_time, NULL);

    while(SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                    SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                    SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS) {
        if (vverbose) {
            fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
            fflush(stdout);
        }
        numberOfPacketsSent    += 1;
        numberOfBytesSent      += length;

        if (numberOfPacketsToSend != 0) {
            if (numberOfPacketsSent > numberOfPacketsToSend) {
                  if (verbose) {
                      fprintf(stdout, "%-8x: %lu bytes sent in %u packets -> shutting down !!!\n", assocID, numberOfBytesSent, numberOfPacketsSent);
                      fflush(stdout);
                  }
                  SCTP_shutdown(assocID);
                  break;
            }
        }
        length = 1 + (random() % 1024);
        memset(buffer, 'A', length);
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
    int length;
    char buffer[BUFFER_LENGTH];

    if (verbose) {  
        fprintf(stdout, "%-8x: Restart\n", assocID);
        fflush(stdout);
    }
    
    length = 1 + (random() % 1024);
    memset(buffer, 'A', length);
    buffer[length-1] = '\n';
    
    while((!(((struct ulp_data *)ulpDataPtr)->ShutdownReceived)) &&
          (SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                     SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                     SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS)) {
      if (vverbose) {
          fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
          fflush(stdout);
      }
      length = 1 + (random() % 1024);
      memset(buffer, 'A', length);
      buffer[length-1] = '\n';
    }
}

void shutdownCompleteNotif(unsigned int assocID, void* ulpDataPtr)
{
    double seconds;
    double bytes_per_second;
    double packets_per_second;

    if (verbose) {  
        fprintf(stdout, "%-8x: Shutdown complete\n", assocID);
        fflush(stdout);
    }
    gettimeofday(&end_time, NULL);
    timersub(&end_time, &start_time, &diff_time);
    seconds = diff_time.tv_sec + (diff_time.tv_usec/1000000);
    bytes_per_second = numberOfBytesSent/seconds;
    packets_per_second = numberOfPacketsSent/seconds;

    if (verbose) {
          fprintf(stdout, "%-8x: %f bytes/sec -- %f packets/sec were sent. Shutting down\n", assocID, bytes_per_second,packets_per_second);
          fflush(stdout);
    }

    free((struct ulp_data *) ulpDataPtr);
    SCTP_deleteAssociation(assocID);
}

void queueStatusChangeNotif(unsigned int assocID, int queueType, int queueID, int queueLength, void* ulpDataPtr)
{	
    int length;
    char buffer[BUFFER_LENGTH];

    if (vverbose) {
        fprintf(stdout, "%-8x: Queue status change notification: Type %d, ID %d, Length %d\n",
                        assocID, queueType, queueID, queueLength);
        fflush(stdout);
    }
    
    /* if (queueType == SCTP_SEND_QUEUE) { */
    if (queueType == SCTP_SEND_QUEUE && queueLength <= SEND_QUEUE_SIZE) { 
      length = 1 + (random() % 1024);
      memset(buffer, 'A', length);
      buffer[length-1] = '\n';
      
      while((!(((struct ulp_data *)ulpDataPtr)->ShutdownReceived)) &&
            (SCTP_send(assocID, 0, buffer, length, SCTP_GENERIC_PAYLOAD_PROTOCOL_ID,
                       SCTP_USE_PRIMARY, SCTP_NO_CONTEXT, timeToLive, 
                       SCTP_ORDERED_DELIVERY, SCTP_BUNDLING_DISABLED) == SCTP_SUCCESS)) {
        if (vverbose) {
            fprintf(stdout, "%-8x: %u bytes sent.\n", assocID, length);
            fflush(stdout);
        }
        numberOfPacketsSent    += 1;
        numberOfBytesSent      += length;
        if (numberOfPacketsToSend != 0) {
            if (numberOfPacketsSent > numberOfPacketsToSend) {
                  if (verbose) {
                      fprintf(stdout, "%-8x: %lu bytes sent in %u packets -> shutting down !!!\n", assocID, numberOfBytesSent, numberOfPacketsSent);
                      fflush(stdout);
                  }
                  SCTP_shutdown(assocID);
                  break;
            }
        }
        length = 1 + (random() % 1024);
        memset(buffer, 'A', length);
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




