/*
 * $Id: streamengine.c,v 1.8 2003/10/28 22:00:15 tuexen Exp $
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
 *
 * Purpose: This modules implements the interface defined in streamengine.h and
 *          holds the private list of streams to process them for reasons of
 *          sending and receiving datachunks.
 *
 */

#include "globals.h"
#include <errno.h>
#include "flowcontrol.h"
#include "streamengine.h"
#include "distribution.h"
#include "bundling.h"
#include "errorhandler.h"
#include "SCTP-control.h"

#include "recvctrl.h"

#include "sctp.h"

#include <glib.h>


/******************** Structure Definitions ****************************************/

typedef struct
{
    GList *pduList;          /*list of PDUs waiting for pickup (after notofication has been called) */
    GList *unorderedList;	 /*list for unordered fragments */
    GList *orderedList;      /*list of ordered chunks AND fragments (waiting for reordering) */
    unsigned int nextSSN;
    int index;
}ReceiveStream;

typedef struct
{
    unsigned int nextSSN;
}SendStream;

typedef struct
{
    unsigned int numSendStreams;
    unsigned int numReceiveStreams;
    ReceiveStream*  RecvStreams;
    SendStream*     SendStreams;
    gboolean*       streamActivated;
    unsigned int    queuedBytes;
    gboolean        unreliable;
}StreamEngine;

/*
 * this stores all the data that might be
 * needed to be delivered to the user
 */
typedef struct _delivery_data
{
    guint8 chunk_flags;
    guint16 data_length;
    guint32 tsn;
    guint16 stream_id;
    guint16 stream_sn;
    guint32 protocolId;
    guint32 fromAddressIndex;
    guchar data[MAX_DATACHUNK_PDU_LENGTH];
}
delivery_data;


typedef struct _delivery_pdu
{
    guint32  number_of_chunks;
    guint32  read_position;
    guint32  read_chunk;
    guint32  chunk_position;
    guint32  total_length;
    guint32  fromAddressIndex;
    /* one chunk pointer or array of these */
    delivery_data** ddata;
}delivery_pdu;



/******************** Declarations *************************************************/
int se_deliverOrderedPDUs(StreamEngine* se, unsigned short sid);
int se_deliverUnorderedPDUs(StreamEngine* se, unsigned short sid);

void print_element(gpointer list_element, gpointer user_data)
{
    delivery_data * one = (delivery_data *)list_element;
    if (one) {
        event_logii (VERBOSE, "chunklist: tsn %u, SID %u",one->tsn, one->stream_id);
    } else {
        event_log (VERBOSE, "chunklist: NULL element ");
    }
}

int sort_tsn_se(delivery_data * one, delivery_data * two)
{
    if (before(one->tsn, two->tsn)) {
        return -1;
    } else if (after(one->tsn, two->tsn)) {
        return 1;
    } else                      /* one==two */
        return 0;
}

/******************** Function Definitions *****************************************/

/* This function is called to instanciate one Stream Engine for an association.
   It creates and initializes the Lists for Sending and Receiving Data.
   It is called by Message Distribution.
   returns: the pointer to the Stream Engine
*/

void* se_new_stream_engine (unsigned int numberReceiveStreams,        /* max of streams to receive */
                            unsigned int numberSendStreams,           /* max of streams to send */
                            gboolean assocSupportsPRSCTP)
{
    unsigned int i;
    StreamEngine* se;

    event_logiii (EXTERNAL_EVENT, "new_stream_engine: #inStreams=%d, #outStreams=%d, unreliable == %s",
            numberReceiveStreams,	numberSendStreams, (assocSupportsPRSCTP==TRUE)?"TRUE":"FALSE");

    se = (StreamEngine*) malloc(sizeof(StreamEngine));

    if (se == NULL) {
        error_log(ERROR_FATAL,"Out of Memory in se_new_stream_engine()");
        return NULL;
    }

    se->RecvStreams = (ReceiveStream*)malloc(numberReceiveStreams*sizeof(ReceiveStream));
    if (se->RecvStreams == NULL) {
        free(se);
        error_log(ERROR_FATAL,"Out of Memory in se_new_stream_engine()");
        return NULL;
    }
    se->streamActivated = (gboolean*)malloc(numberReceiveStreams*sizeof(gboolean));
    if (se->streamActivated == NULL) {
        free(se->RecvStreams);
        free(se);
        error_log(ERROR_FATAL,"Out of Memory in se_new_stream_engine()");
        return NULL;
    }

    for (i=0; i<numberReceiveStreams; i++) se->streamActivated[i] = FALSE;

    se->SendStreams = (SendStream*)malloc(numberSendStreams*sizeof(SendStream));
    if (se->SendStreams == NULL) {
        free(se->RecvStreams);
				free(se->streamActivated);
        free(se);
        error_log(ERROR_FATAL,"Out of Memory in se_new_stream_engine()");
        return NULL;
    }

    se->numSendStreams = numberSendStreams;
    se->numReceiveStreams = numberReceiveStreams;
    se->unreliable = assocSupportsPRSCTP;

    for (i = 0; i < numberReceiveStreams; i++)
    {
      (se->RecvStreams)[i].nextSSN = 0;
      (se->RecvStreams)[i].pduList = NULL;
      (se->RecvStreams[i]).unorderedList = NULL;
      (se->RecvStreams[i]).orderedList = NULL;
      (se->RecvStreams[i]).index = 0; /* for ordered chunks, next ssn */
    }
    for (i = 0; i < numberSendStreams; i++)
    {
      (se->SendStreams[i]).nextSSN = 0;
    }

    se->queuedBytes = 0;

    return (se);
}



/* Deletes the instance pointed to by streamengine.
*/
void
se_delete_stream_engine (void *septr)
{
  StreamEngine* se;
  unsigned int i;
  se = (StreamEngine*) septr;

  event_log (INTERNAL_EVENT_0, "delete streamengine: freeing send streams");
  free(se->SendStreams);
  for (i = 0; i < se->numReceiveStreams; i++)
    {
        event_logi (VERBOSE, "delete streamengine: freeing data for receive stream %d",i);
        /* whatever is still in these lists, delete it before freeing the lists */
        g_list_foreach(se->RecvStreams[i].pduList, &free_list_element, NULL);
        g_list_foreach(se->RecvStreams[i].unorderedList, &free_list_element, NULL);
        g_list_foreach(se->RecvStreams[i].orderedList, &free_list_element, NULL);
        g_list_free(se->RecvStreams[i].pduList);
        g_list_free(se->RecvStreams[i].unorderedList);
        g_list_free(se->RecvStreams[i].orderedList);
    }
  event_log (INTERNAL_EVENT_0, "delete streamengine: freeing receive streams");
  free(se->RecvStreams);
  free(se->streamActivated);
  free (se);
  event_log (EXTERNAL_EVENT, "deleted streamengine");
}



int
se_readNumberOfStreams (unsigned short *inStreams, unsigned short *outStreams)
{
  StreamEngine* se = (StreamEngine*) mdi_readStreamEngine ();
  if (se == NULL)
    {
      error_log(ERROR_MINOR, "Called se_readNumberOfStreams, but no Streamengine is there !");
      *inStreams = 0;
      *outStreams = 0;
      return -1;
    }
  *inStreams = se->numReceiveStreams;
  *outStreams = se->numSendStreams;
  return 0;
}


/******************** Functions for Sending *****************************************/

/**
 * This function is called to send a chunk.
 *  called from MessageDistribution
 * @return 0 for success, -1 for error (e.g. data sent in shutdown state etc.)
*/
int
se_ulpsend (unsigned short streamId, unsigned char *buffer,
            unsigned int byteCount,  unsigned int protocolId,
            short destAddressIndex, void *context, unsigned int lifetime,
            gboolean unorderedDelivery, gboolean dontBundle)
{
    StreamEngine* se=NULL;
    guint32 state;
    chunk_data*  cdata=NULL;
    SCTP_data_chunk* dchunk=NULL;
    unsigned char* bufPosition = buffer;

    unsigned int bCount = 0, maxQueueLen = 0;
    int numberOfSegments, residual;

    int i = 0;
    int result = 0, retVal;


    state = sci_getState ();
    if (sci_shutdown_procedure_started () == TRUE)
    {
        event_logi (EXTERNAL_EVENT,
        "se_ulpsend: Cannot send Chunk, Association (state==%u) in SHUTDOWN-phase", state);
        return SCTP_SPECIFIC_FUNCTION_ERROR;
    }

    event_logii (EXTERNAL_EVENT, "se_ulpsend : %u bytes for stream %u", byteCount,streamId);

    se = (StreamEngine*) mdi_readStreamEngine ();
    if (se == NULL)
    {
        error_log (ERROR_MAJOR, "se_ulpsend: StreamEngine Instance doesn't exist....Returning !");
        return SCTP_MODULE_NOT_FOUND;
    }

    if (streamId >= se->numSendStreams)
    {
        error_logii (ERROR_MAJOR, "STREAM ID OVERFLOW in se_ulpsend: wanted %u, got only %u",
            streamId, se->numSendStreams);
        mdi_sendFailureNotif (buffer, byteCount, context);
        return SCTP_PARAMETER_PROBLEM;
    }

    result = fc_get_maxSendQueue(&maxQueueLen);
    if (result != SCTP_SUCCESS) return SCTP_UNSPECIFIED_ERROR;


    retVal = SCTP_SUCCESS;

    /* if (byteCount <= fc_getMTU())          */
    if (byteCount <= SCTP_MAXIMUM_DATA_LENGTH)
    {
       if (maxQueueLen > 0) {
         if ((1 + fc_readNumberOfQueuedChunks()) > maxQueueLen) return SCTP_QUEUE_EXCEEDED;
       }

        cdata = malloc(sizeof(chunk_data));
        if (cdata == NULL) {
            return SCTP_OUT_OF_RESOURCES;
        }

        dchunk = (SCTP_data_chunk*)cdata->data;

        dchunk->chunk_id      = CHUNK_DATA;
        dchunk->chunk_flags   = SCTP_DATA_BEGIN_SEGMENT + SCTP_DATA_END_SEGMENT;
        dchunk->chunk_length  = htons (byteCount + FIXED_DATA_CHUNK_SIZE);
        dchunk->tsn = 0;        /* gets assigned in the flowcontrol module */
        dchunk->stream_id     = htons (streamId);
        dchunk->protocolId    = htonl (protocolId);

        if (unorderedDelivery)
        {
            dchunk->stream_sn = htons (0);
            dchunk->chunk_flags += SCTP_DATA_UNORDERED;
        }
        else
        {       /* unordered flag not put */
            dchunk->stream_sn = htons (se->SendStreams[streamId].nextSSN);
            se->SendStreams[streamId].nextSSN++;
            se->SendStreams[streamId].nextSSN = se->SendStreams[streamId].nextSSN % 0x10000;
        }
        /* copy the data, but only once ! */
        memcpy (dchunk->data, buffer, byteCount);

        event_logii (EXTERNAL_EVENT, "=========> ulp sent a chunk (SSN=%u, SID=%u) to StreamEngine <=======",
                      ntohs (dchunk->stream_sn),ntohs (dchunk->stream_id));
        if (!se->unreliable) lifetime = 0xFFFFFFFF;
        result = fc_send_data_chunk (cdata, destAddressIndex, lifetime, dontBundle, context);

        if (result != SCTP_SUCCESS)	{
            error_logi (ERROR_MINOR, "se_ulpsend() failed with result %d", result);
            return result;
        }
    }
    else
    {
        /* calculate nr. of necessary chunks -> use fc_getMTU() later !!! */
      numberOfSegments = byteCount / SCTP_MAXIMUM_DATA_LENGTH;	
      residual = byteCount % SCTP_MAXIMUM_DATA_LENGTH;
      if (residual != 0) {
            numberOfSegments++;
      } else {
            residual = SCTP_MAXIMUM_DATA_LENGTH;
      }

      if (maxQueueLen > 0) {
        if ((numberOfSegments + fc_readNumberOfQueuedChunks()) > maxQueueLen) return SCTP_QUEUE_EXCEEDED;
      }

      for (i = 1; i <= numberOfSegments; i++)
      {
            cdata = malloc(sizeof(chunk_data));
            if (cdata == NULL) {
                /* FIXME: this is unclean, as we have already assigned some TSNs etc, and
                 * maybe queued parts of this message in the queue, this should be cleaned
                 * up... */
                return SCTP_OUT_OF_RESOURCES;
            }

            dchunk = (SCTP_data_chunk*)cdata->data;

            if ((i != 1) && (i != numberOfSegments))
	            {
	                dchunk->chunk_flags = 0;
              	    bCount = SCTP_MAXIMUM_DATA_LENGTH;
	                event_log (VERBOSE, "NEXT FRAGMENTED CHUNK -> MIDDLE");
	            }
	        else if (i == 1)
	            {
	                dchunk->chunk_flags = SCTP_DATA_BEGIN_SEGMENT;
	                event_log (VERBOSE, "NEXT FRAGMENTED CHUNK -> BEGIN");
            	    bCount = SCTP_MAXIMUM_DATA_LENGTH;
	            }
	        else if (i == numberOfSegments)
	            {
	                dchunk->chunk_flags = SCTP_DATA_END_SEGMENT;
	                event_log (EXTERNAL_EVENT, "NEXT FRAGMENTED CHUNK -> END");
            	    bCount = residual;
	            }

	        dchunk->chunk_id = CHUNK_DATA;
	        dchunk->chunk_length = htons (bCount + FIXED_DATA_CHUNK_SIZE);
	        dchunk->tsn = htonl (0);
	        dchunk->stream_id = htons (streamId);
	        dchunk->protocolId = htonl (protocolId);
	
    	    if (unorderedDelivery)
	            {
	                dchunk->stream_sn = 0;
	                dchunk->chunk_flags += SCTP_DATA_UNORDERED;
	            }
	        else
	            {			/* unordered flag not put */
		            dchunk->stream_sn = htons (se->SendStreams[streamId].nextSSN);
                    /* only after the last segment we increase the SSN */
                if (i == numberOfSegments) {
	            		se->SendStreams[streamId].nextSSN++;
  	     	        se->SendStreams[streamId].nextSSN = se->SendStreams[streamId].nextSSN % 0x10000;
	  	     	    }
	            }
	
          memcpy (dchunk->data, bufPosition, bCount);
          bufPosition += bCount * sizeof(unsigned char);
	

	        event_logiii (EXTERNAL_EVENT, "======> SE sends fragment %d of chunk (SSN=%u, SID=%u) to FlowControl <======",
		            i, ntohs (dchunk->stream_sn),ntohs (dchunk->stream_id));

            if (!se->unreliable) lifetime = 0xFFFFFFFF;

            result = fc_send_data_chunk (cdata, destAddressIndex, lifetime, dontBundle, context);

            if (result != SCTP_SUCCESS) {
                error_logi (ERROR_MINOR, "se_ulpsend() failed with result %d", result);
                /* FIXME : Howto Propagate an Error here - Result gets overwritten on next Call */
                retVal = result;
            }
	    }
    }
  return retVal;
}


/******************** Functions for Receiving **************************************/

/**
 * This function is called from distribution layer to receive a chunk.
 */
short se_ulpreceivefrom(unsigned char *buffer, unsigned int *byteCount,
                        unsigned short streamId, unsigned short* streamSN,
                        unsigned int * tsn, unsigned int* addressIndex, unsigned int flags)
{

  delivery_pdu  *d_pdu = NULL;
  unsigned int copiedBytes, residual, i;
  guint32 r_pos, r_chunk, chunk_pos, oldQueueLen = 0;


  StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();


  if (se == NULL)
    {
      error_log (ERROR_MAJOR, "Could not retrieve SE instance ");
      return SCTP_MODULE_NOT_FOUND;
    }
  if (buffer == NULL || byteCount == NULL)
    {
      error_log (ERROR_MAJOR, "Wrong Arguments : Pointers are NULL");
      return SCTP_PARAMETER_PROBLEM;
    }

  if (streamId >= se->numReceiveStreams)
    {
      error_log (ERROR_MINOR, "STREAM ID OVERFLOW");
      return (STREAM_ID_OVERFLOW);
    }
  else
    {
      event_logii (EXTERNAL_EVENT, "SE_ULPRECEIVE (sid: %u, numBytes: %u) CALLED",streamId,*byteCount);

      if (se->RecvStreams[streamId].pduList == NULL)
        {
            event_log (EXTERNAL_EVENT, "NO DATA AVAILABLE");
            return (NO_DATA_AVAILABLE);
        }
      else
        {
            oldQueueLen = se->queuedBytes;
            copiedBytes = 0;

            d_pdu = g_list_nth_data (se->RecvStreams[streamId].pduList, 0);

            r_pos       = d_pdu->read_position;
            r_chunk     = d_pdu->read_chunk;
            chunk_pos   = d_pdu->chunk_position;

            *streamSN   = d_pdu->ddata[d_pdu->read_chunk]->stream_sn;
            *tsn        = d_pdu->ddata[d_pdu->read_chunk]->tsn;
            *addressIndex = d_pdu->fromAddressIndex;

            event_logiiii (VVERBOSE, "SE_ULPRECEIVE (read_position: %u, read_chunk: %u, chunk_position: %u, total_length: %u)",
                    r_pos,  r_chunk, chunk_pos, d_pdu->total_length);

            if (d_pdu->total_length - d_pdu->read_position < *byteCount)
                *byteCount = d_pdu->total_length-d_pdu->read_position;

            residual = *byteCount;

            while (copiedBytes < *byteCount) {

                if (d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position > residual) {
                    event_logiii (VVERBOSE, "Copy in SE_ULPRECEIVE (residual: %u, copied bytes: %u, byteCount: %u)",
                        residual, copiedBytes,*byteCount);

                    memcpy (&buffer[copiedBytes],
                            &(d_pdu->ddata[d_pdu->read_chunk]->data)[d_pdu->chunk_position],
                            residual);

                    d_pdu->chunk_position += residual;
                    d_pdu->read_position  += residual;
                    copiedBytes           += residual;
                    residual = 0;
                } else {
                    event_logi (VVERBOSE, "Copy in SE_ULPRECEIVE (num: %u)",d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position);

                    memcpy (&buffer[copiedBytes],
                            &(d_pdu->ddata[d_pdu->read_chunk]->data)[d_pdu->chunk_position],
                            d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position);

                    d_pdu->read_position += (d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position);
                    copiedBytes          += (d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position);
                    residual             -= (d_pdu->ddata[d_pdu->read_chunk]->data_length - d_pdu->chunk_position);
                    d_pdu->chunk_position = 0;
                    d_pdu->read_chunk++;
                }
            }

            if (flags == SCTP_MSG_PEEK) {
                d_pdu->chunk_position   = chunk_pos;
                d_pdu->read_position    = r_pos;
                d_pdu->read_chunk       = r_chunk;
            } else {

               if (d_pdu->read_position >= d_pdu->total_length) {

                    se->queuedBytes -= d_pdu->total_length;

                    se->RecvStreams[streamId].pduList =
                        g_list_remove (se->RecvStreams[streamId].pduList,
                                       g_list_nth_data (se->RecvStreams[streamId].pduList, 0));
                    event_log (VERBOSE, "Remove PDU element from the SE list, and free associated memory");
                    for (i=0; i < d_pdu->number_of_chunks; i++) free(d_pdu->ddata[i]);
                    free(d_pdu->ddata);
                    free(d_pdu);
                    rxc_start_sack_timer(oldQueueLen);
                }
            }

        }

    }
    event_logi (EXTERNAL_EVENT, "ulp receives %u bytes from se", *byteCount);
    return (RECEIVE_DATA);
}

/*
 * function that gets chunks from the Lists, transforms them to PDUs, puts them
 * to the pduList, and calls DataArrive-Notification
 */
int se_doNotifications(void){
    int retVal, result;
    unsigned int i;
    
    StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();

    if (se == NULL) {
      error_log (ERROR_MAJOR, "Could not retrieve SE instance ");
      return SCTP_MODULE_NOT_FOUND;
    }

    event_log (INTERNAL_EVENT_0, " ================> se_doNotifications <=============== ");

    retVal = SCTP_SUCCESS;
    for (i = 0; i < se->numReceiveStreams; i++) {
        if (se->streamActivated[i] == TRUE) {
            if (se->RecvStreams[i].unorderedList != NULL) result = se_deliverUnorderedPDUs(se, i);

            if (se->RecvStreams[i].orderedList != NULL)
                while ((result = se_deliverOrderedPDUs(se, i)) > 0) {}

            se->streamActivated[i] = FALSE;
        }
    }
    event_log (INTERNAL_EVENT_0, " ================> se_doNotifications: DONE <=============== ");
    return retVal;
}


/*
 * This function is called from Receive Control to forward received chunks to Stream Engine.
 * send error chunk, when maximum stream id is exceeded !
 */
int
se_recvDataChunk(SCTP_data_chunk * dataChunk, unsigned int byteCount, unsigned int address_index)
{
    guint16 datalength;
    delivery_data* d_chunk;
    SCTP_InvalidStreamIdError error_info;
    StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();

    if (se == NULL) {
      error_log (ERROR_MAJOR, "Could not retrieve SE instance ");
      return SCTP_MODULE_NOT_FOUND;
    }

    event_log (INTERNAL_EVENT_0, "SE_RECVDATACHUNK CALLED");

    d_chunk = malloc (sizeof (delivery_data));
    if (d_chunk == NULL) return SCTP_OUT_OF_RESOURCES;
    d_chunk->stream_id =    ntohs (dataChunk->stream_id);
    datalength =  byteCount - FIXED_DATA_CHUNK_SIZE;
    
    if (d_chunk->stream_id >= se->numReceiveStreams) {
        /* return error, when numReceiveStreams is exceeded */
        error_info.stream_id = htons(d_chunk->stream_id);
        error_info.reserved = htons(0);
        scu_abort(ECC_INVALID_STREAM_ID, sizeof(error_info), (void*)&error_info);
        free(d_chunk);
        return SCTP_UNSPECIFIED_ERROR;
    }

    d_chunk->tsn = ntohl (dataChunk->tsn);     /* for efficiency */

    if (datalength <= 0) {
        scu_abort(ECC_NO_USER_DATA, sizeof(unsigned int), (void*)&(dataChunk->tsn));
        free(d_chunk);
        return SCTP_UNSPECIFIED_ERROR;
    }

    memcpy (d_chunk->data, dataChunk->data, datalength);
    d_chunk->data_length = datalength;
    d_chunk->tsn = ntohl (dataChunk->tsn);     /* for efficiency */
    d_chunk->chunk_flags = dataChunk->chunk_flags;
    d_chunk->stream_sn =    ntohs (dataChunk->stream_sn);
    d_chunk->protocolId =   ntohl (dataChunk->protocolId);
    d_chunk->fromAddressIndex =  address_index;
    

    if ((d_chunk->chunk_flags >= 0x04) && (d_chunk->chunk_flags < 0x08)) {
        event_logii (EXTERNAL_EVENT, "se_recvDataChunk: UNORDERED chunk, tsn: %u, sid: %u", d_chunk->tsn, d_chunk->stream_id);
        se->RecvStreams[d_chunk->stream_id].unorderedList =
           g_list_insert_sorted (se->RecvStreams[d_chunk->stream_id].unorderedList, d_chunk, (GCompareFunc) sort_tsn_se);
    } else if (d_chunk->chunk_flags <= 0x03) {
        event_logiiiii (EXTERNAL_EVENT, "se_recvDataChunk: ORDERED chunk, tsn: %u, sid: %u, ssn: %u, list: %x, length: %u",
            d_chunk->tsn,d_chunk->stream_id,d_chunk->stream_sn, se->RecvStreams[d_chunk->stream_id].orderedList,
                g_list_length(se->RecvStreams[d_chunk->stream_id].orderedList));
        se->RecvStreams[d_chunk->stream_id].orderedList =
            g_list_insert_sorted (se->RecvStreams[d_chunk->stream_id].orderedList, d_chunk, (GCompareFunc) sort_tsn_se);
    } else {
        /* FIXME : Return error, when Flags are invalid */
        scu_abort(ECC_OUT_OF_RESOURCE_ERROR, 0, NULL);
        free(d_chunk);
        return SCTP_UNSPECIFIED_ERROR;
    }
    se->queuedBytes += datalength;
    se->streamActivated[d_chunk->stream_id] = TRUE;
    return 0;
}


int se_deliverOrderedPDUs(StreamEngine* se, unsigned short sid)
{
    gboolean beginFound = FALSE, endFound = FALSE, ssnOkay= FALSE, newStarted = TRUE;
    int chunksToDeliver = 1, i;
    unsigned int total_length =  0;
    GList* lst = se->RecvStreams[sid].orderedList;
    GList* tmp = g_list_first(lst);
    delivery_data* d_chunk;
    delivery_pdu*  d_pdu;
    unsigned int   tmp_tsn = 0;
    unsigned short tmp_ssn = 0;

    /* tmp->data is a d_chunk struct */
    event_logi (INTERNAL_EVENT_0, " ================> se_deliverOrderedPDU(sid: %u) <=============== ", sid);

    while (tmp != NULL) {
        d_chunk = (delivery_data*)(tmp->data);
        tmp_ssn = d_chunk->stream_sn;
        if (newStarted == TRUE) {
            /* set this once per delivered pdu (and) notification */
            tmp_tsn = d_chunk->tsn - 1;
            newStarted = FALSE;
        }
        event_logiii(VVERBOSE, "se_deliverOrderedPDU: check chunk (ssn: %u, tsn: %u, tmp_tsn: %u)",tmp_ssn,d_chunk->tsn,tmp_tsn);

        if (beginFound != TRUE) {
            /* first chunk MUST have a begin flag ! */
            if (d_chunk->chunk_flags & SCTP_DATA_BEGIN_SEGMENT) {
                beginFound = TRUE;
            }
            else return 0;
        }

        if (d_chunk->chunk_flags & SCTP_DATA_END_SEGMENT) endFound = TRUE;

        if (beginFound == TRUE) {
            /* we have at least two fragments, that must be TSNa, TSNa+1, etc. */
            if (tmp_tsn + 1 == d_chunk->tsn) tmp_tsn++;
            else return 0;
        }

        if (tmp_ssn == se->RecvStreams[sid].nextSSN) {
            ssnOkay = TRUE;
            if (endFound == TRUE) {
               se->RecvStreams[sid].nextSSN++;
               se->RecvStreams[sid].nextSSN = se->RecvStreams[sid].nextSSN % 0x10000;
            }
        } else {
            event_logiii(VVERBOSE, "se_deliverOrderedPDU(sid: %u): did not find matching SSN (expect: %u, was: %u)",
                sid, se->RecvStreams[sid].nextSSN, tmp_ssn);
            ssnOkay = FALSE;
        }

        if (beginFound == TRUE && endFound == TRUE && ssnOkay == TRUE) {

            d_pdu = malloc(sizeof(delivery_pdu));
            if (d_pdu == NULL) {
                return SCTP_OUT_OF_RESOURCES;
            }
            d_pdu->number_of_chunks = chunksToDeliver;
            d_pdu->read_position = 0;
            d_pdu->read_chunk = 0;
            d_pdu->chunk_position = 0;

            d_pdu->ddata = malloc(chunksToDeliver*sizeof(delivery_data*));
            if (d_pdu->ddata == NULL) {
                free(d_pdu);
                return SCTP_OUT_OF_RESOURCES;
            }

            /* get pointers to the first chunks and put them into the pduList */
            for (i = 0; i < chunksToDeliver; i++) {
                d_pdu->ddata[i] = (delivery_data*)g_list_nth_data(se->RecvStreams[sid].orderedList, i);
                total_length += d_pdu->ddata[i]->data_length;
            }
            tmp_tsn =  d_pdu->ddata[0]->tsn;
            d_pdu->fromAddressIndex = d_pdu->ddata[0]->fromAddressIndex;
            d_pdu->total_length = total_length;                        
            /* assemble delivery_pdu, and Notify */
            se->RecvStreams[sid].pduList = g_list_append(se->RecvStreams[sid].pduList, d_pdu);
            mdi_dataArriveNotif(sid, total_length, tmp_ssn, tmp_tsn, d_pdu->ddata[0]->protocolId, 0);
            /* remove chunks from the list and return */
            for (i = 0; i < chunksToDeliver; i++) {
                 se->RecvStreams[sid].orderedList = g_list_remove(se->RecvStreams[sid].orderedList,
                                                    g_list_nth_data(se->RecvStreams[sid].orderedList,0));
            }
            return chunksToDeliver;

        } else if (beginFound == TRUE && endFound == FALSE && ssnOkay == TRUE) {
            chunksToDeliver++;
        } else {    /* ssnOkay == FALSE */
            return 0;
        }
        tmp = g_list_next(tmp);

    }
    return 0;
}

int se_deliverUnorderedPDUs(StreamEngine* se, unsigned short sid)
{
    gboolean beginFound = FALSE, endFound = FALSE, newStarted = TRUE;
    int chunksToDeliver = 1, i;
    unsigned int total_length =  0;
    GList* lst = se->RecvStreams[sid].unorderedList;
    GList* tmp = g_list_first(lst);
    GList* start = NULL;
    delivery_data* d_chunk;
    delivery_pdu*  d_pdu;
    unsigned int   tmp_tsn = 0, del_tsn = 0;

    /* tmp->data is a d_chunk struct */
    event_logi (VERBOSE, " ================> se_deliverUnorderedPDU(sid: %u) <=============== ", sid);

    while (tmp != NULL) {

        d_chunk = (delivery_data*)(tmp->data);

        event_logii (VERBOSE, " se_deliverUnorderedPDU(treat chunk TSN: %u, SSN: %u)", d_chunk->tsn, d_chunk->stream_sn);

        if (d_chunk->chunk_flags & SCTP_DATA_BEGIN_SEGMENT) {
            event_log (VERBOSE, " se_deliverUnorderedPDU( -> beginFound !)");
            beginFound = TRUE;
            start = tmp;
        }

        if (beginFound == TRUE) {
            if (newStarted == TRUE) {
                /* set this once per delivered pdu (and) notification */
                tmp_tsn = d_chunk->tsn - 1;
                newStarted = FALSE;
            }
            if (d_chunk->chunk_flags & SCTP_DATA_END_SEGMENT) endFound = TRUE;

            if (tmp_tsn + 1 == d_chunk->tsn) {
                tmp_tsn++;
            } else {
                tmp = g_list_next(tmp);
                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; start = NULL; total_length = 0;
                chunksToDeliver = 1;
                continue;
            }
            /* TSN okay, endFound true or false */

            if (endFound == TRUE) {
                event_logii (VERBOSE, " se_deliverUnorderedPDU( -> endFound, TSN: %u, %u chunks !)", d_chunk->tsn, chunksToDeliver);

                d_pdu = malloc(sizeof(delivery_pdu));
                if (d_pdu == NULL) {
                    return SCTP_OUT_OF_RESOURCES;
                }
                d_pdu->number_of_chunks = chunksToDeliver;
                d_pdu->read_position = 0;
                d_pdu->read_chunk = 0;
                d_pdu->chunk_position = 0;

                d_pdu->ddata = malloc(chunksToDeliver*sizeof(delivery_data*));
                if (d_pdu->ddata == NULL) {
                    free(d_pdu);
                    return SCTP_OUT_OF_RESOURCES;
                }
                /* get pointers to the chunks that will be delivered and put them into the pduList */
                tmp = start;
                for (i = 0; i < chunksToDeliver; i++) {
                    d_pdu->ddata[i] = tmp->data;
                    total_length += d_pdu->ddata[i]->data_length;
                    tmp = g_list_next(tmp);
                }
                d_pdu->total_length = total_length;
                del_tsn = d_pdu->ddata[0]->tsn;
                d_pdu->fromAddressIndex = d_pdu->ddata[0]->fromAddressIndex;
                
                /* remove chunks from the list */
                for (i = 0; i < chunksToDeliver; i++) {
                     se->RecvStreams[sid].unorderedList = g_list_remove(se->RecvStreams[sid].unorderedList,
                                                                        d_pdu->ddata[i]);
                }

                /* assemble delivery_pdu, and Notify */
                se->RecvStreams[sid].pduList = g_list_append(se->RecvStreams[sid].pduList, d_pdu);
                mdi_dataArriveNotif(sid, total_length, 0, del_tsn, d_pdu->ddata[0]->protocolId, 1);

                event_logi (VERBOSE, " se_deliverUnorderedPDU( data delivered, total_length: %u)", total_length);

                if (tmp == NULL)
                    event_log (VERBOSE, " se_deliverUnorderedPDU(all chunks delivered)");


                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; total_length = 0;
                chunksToDeliver = 1;
                continue;
            } else {          /* endFound == FALSE */
                chunksToDeliver++;
            }
        } else {    /* beginFound == FALSE */
            tmp = g_list_next(tmp);
        }

    }   /* end while () */
    return 0;
}


/**
 * function to return the number of chunks that can be retrieved
 * by the ULP - this function may need to be refined !!!!!!
 */
guint32 se_numOfQueuedChunks ()
{
  guint32 i, num_of_chunks = 0;
  StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();

  if (se == NULL)
    {
      error_log (ERROR_MAJOR, "Could not read StreamEngine Instance !");
      return 0xFFFFFFFF;
    }

  for (i = 0; i < se->numReceiveStreams; i++)
    {
      /* Add number of all chunks (i.e. lengths of all pduList lists of all streams */
      num_of_chunks += g_list_length (se->RecvStreams[i].pduList);
    }
  return num_of_chunks;
}



/**
 * function to return the number of streams that we may
 * send on
 */
guint16
se_numOfSendStreams ()
{
  StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();
  if (se == NULL)
    {
      error_log (ERROR_MAJOR, "Could not read StreamEngine Instance !");
      return 0;
    }
  return (guint16) (se->numSendStreams);

}

/**
 * function to return the number of streams that we are allowed to
 * receive data on
 */
guint16
se_numOfRecvStreams ()
{
  StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();
  if (se == NULL)
    {
      error_log (ERROR_MAJOR, "Could not read StreamEngine Instance !");
      return 0;
    }

  return (guint16) (se->numReceiveStreams);

}

gboolean se_deliver_stream_unreliably_ordered(StreamEngine* se, int sid, unsigned short skippedSSN, unsigned int up_to_tsn)
{
    /* send data arrive notif, and put messages into delivery queue  */
    gboolean beginFound = FALSE, endFound = FALSE, newStarted = TRUE, tsnOkay = FALSE;
    unsigned int start_tsn = 0;
    unsigned int chunksToDeliver = 1;
    unsigned int total_length =  0;

    GList* tmp = g_list_first(se->RecvStreams[sid].orderedList);
    GList* start = NULL;

    delivery_data* d_chunk;
    delivery_pdu*  d_pdu;
    unsigned short tmp_ssn = 0;
    int delivered_ssn = -1;
    gboolean advancedSSN = FALSE;
    unsigned short actualSSN;
    unsigned int tmp_tsn =  0, i = 0,current_tsn=0, del_tsn = 0;

    /* tmp->data is a d_chunk struct */
    event_logii (INTERNAL_EVENT_0, "  se_deliver_stream_unreliably_ordered(sid: %u, tsn:%u) ", sid,up_to_tsn);

    while (tmp != NULL) {

        event_log (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST AT THE LOOP START");
        g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);

        d_chunk = (delivery_data*)(tmp->data);
        tmp_ssn = d_chunk->stream_sn;
        current_tsn = d_chunk->tsn;
        if (after(current_tsn, up_to_tsn)) break;

        event_logiii (VERBOSE, "se_deliver_stream_unreliably_ordered: ssn: %u, tsn: %u, up_to_tsn: %u", tmp_ssn, current_tsn,up_to_tsn);

        if (d_chunk->chunk_flags & SCTP_DATA_BEGIN_SEGMENT) {
            event_log (VERBOSE, "(se_deliver_stream_unreliably_ordered -> beginFound !)");
            beginFound = TRUE;
        }
        /* delete until start found */
        if (beginFound == FALSE) {
            event_log (VERBOSE, "(se_deliver_stream_unreliably_ordered -> remove ONE chunk from start !)");
            se->queuedBytes -=  d_chunk->data_length;
            free_list_element(d_chunk, NULL);
            se->RecvStreams[sid].orderedList= g_list_remove(se->RecvStreams[sid].orderedList,
                                                            g_list_nth_data(se->RecvStreams[sid].orderedList,0));
            g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);
            tmp = g_list_first(se->RecvStreams[sid].orderedList);
            /* continue with the first chunk in this list */
            continue;
        } else {
            if (newStarted == TRUE) {
                /* set this once per delivered pdu (and) notification */
                start_tsn = d_chunk->tsn;
                tmp_tsn =   d_chunk->tsn - 1;
                newStarted = FALSE;
            }
            if (d_chunk->chunk_flags & SCTP_DATA_END_SEGMENT) {
                event_log (VERBOSE, "(se_deliver_stream_unreliably_ordered -> endFound !)");
                endFound = TRUE;
            }

            if (current_tsn == tmp_tsn + 1){
                event_logi (VERBOSE, "se_deliver_stream_unreliably_ordered -> tsnOkay, tmp_tsn=%u !", tmp_tsn);
                tsnOkay = TRUE;
                tmp_tsn++;
            } else {
                /* delete all dangling chunks before current tsn */
                event_log (VERBOSE, "(se_deliver_stream_unreliably_ordered -> tsnOkay == FALSE !)");
                tsnOkay = FALSE;
                for (i = 1; i < chunksToDeliver; i++) {
                    if (g_list_nth_data(se->RecvStreams[sid].orderedList,0)) {
                        d_chunk = (delivery_data*) g_list_nth_data(se->RecvStreams[sid].orderedList,0);
                        se->queuedBytes -=  d_chunk->data_length;
                        /* free data also */
                        free_list_element(d_chunk, NULL);
                        se->RecvStreams[sid].orderedList = g_list_remove(se->RecvStreams[sid].orderedList, d_chunk);
                    }
                }
                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; start = NULL; total_length=0;
                chunksToDeliver=1;
                tmp = g_list_first(se->RecvStreams[sid].orderedList);
                /* start over with the same chunk */
                continue;
            }
            event_log (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST HALFWAY THROUGH THE LOOP ");
            g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);

            /* now deliver data to ULP, if we can do it */
            if (beginFound == TRUE && endFound == TRUE && tsnOkay == TRUE) {
                /* deliver data, and set next SSN */
                d_pdu = malloc(sizeof(delivery_pdu));
                if (d_pdu == NULL) {
                    return SCTP_OUT_OF_RESOURCES;
                }
                d_pdu->number_of_chunks = chunksToDeliver;
                d_pdu->read_position = 0;
                d_pdu->read_chunk = 0;
                d_pdu->chunk_position = 0;

                d_pdu->ddata = malloc(chunksToDeliver*sizeof(delivery_data*));
                if (d_pdu->ddata == NULL) {
                    free(d_pdu);
                    return SCTP_OUT_OF_RESOURCES;
                }
                event_logi(VERBOSE, "se_deliver_stream_unreliably_ordered -> deliver %u chunk-fragment", chunksToDeliver);
                event_log (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST BEFORE DATA ARRIVE ");
                g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);

                /* get pointers to the first chunks and put them into the pduList */
                for (i = 0; i < chunksToDeliver; i++) {
                    d_pdu->ddata[i] = (delivery_data*)g_list_nth_data(se->RecvStreams[sid].orderedList, i);
                    total_length += d_pdu->ddata[i]->data_length;
                }
                d_pdu->total_length = total_length;
                del_tsn = d_pdu->ddata[0]->tsn;
                d_pdu->fromAddressIndex = d_pdu->ddata[0]->fromAddressIndex;
                
                /* assemble delivery_pdu, and Notify */
                se->RecvStreams[sid].pduList = g_list_append(se->RecvStreams[sid].pduList, d_pdu);

                event_log (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST AFTER APPENDING ");
                g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);
                /* remove chunks from the list and return */
                for (i = 0; i < chunksToDeliver; i++) {
                    event_logi (VERBOSE, "remove delivered chunk-fragment %u from queue", i+1);
                    se->RecvStreams[sid].orderedList = g_list_remove(se->RecvStreams[sid].orderedList,
                                                       g_list_nth_data(se->RecvStreams[sid].orderedList,0));
                }
                event_logi (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST %x AFTER REMOVAL", se->RecvStreams[sid].orderedList);
                g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);

                mdi_dataArriveNotif(sid, total_length, tmp_ssn, del_tsn, d_pdu->ddata[0]->protocolId, 0);

                delivered_ssn = tmp_ssn;
                event_logi (VERBOSE, "se_deliver_stream_unreliably_ordered -> LIST %x AFTER DATA ARRIVE", se->RecvStreams[sid].orderedList);
                g_list_foreach(se->RecvStreams[sid].orderedList, &print_element, NULL);

                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; total_length = 0;
                chunksToDeliver = 1;
                tmp = g_list_first(se->RecvStreams[sid].orderedList);
                continue;

            } else if (beginFound == TRUE && endFound == FALSE && tsnOkay == TRUE) {
                chunksToDeliver++;
            }
        }
        tmp = g_list_next(tmp);
    }   /* end while */
    if (delivered_ssn != -1) {
        actualSSN = (unsigned short)delivered_ssn;
        advancedSSN = TRUE;
        if (sAfter(skippedSSN, actualSSN)) {
            se->RecvStreams[sid].nextSSN = skippedSSN;
        } else {
            se->RecvStreams[sid].nextSSN = actualSSN;
        }
    } else {
        if (sAfter(skippedSSN, se->RecvStreams[sid].nextSSN) || (skippedSSN==se->RecvStreams[sid].nextSSN)) {
            se->RecvStreams[sid].nextSSN = skippedSSN;
            advancedSSN = TRUE;
        } else {
            advancedSSN = FALSE;
        }
    }
    return advancedSSN;
}


unsigned short se_deliver_stream_unreliably_unordered(StreamEngine* se, int sid, unsigned short skippedSSN, unsigned int up_to_tsn)
{
    /* send data arrive notif, and put messages into delivery queue  */
    gboolean beginFound = FALSE, endFound = FALSE, newStarted = TRUE, tsnOkay = FALSE;
    unsigned int start_tsn = 0;
    unsigned int chunksToDeliver = 1;
    unsigned int total_length =  0;

    GList* tmp = g_list_first(se->RecvStreams[sid].unorderedList);
    GList* start = NULL;

    delivery_data* d_chunk;
    delivery_pdu*  d_pdu;
    unsigned short tmp_ssn = 0, delivered_ssn = 0;
    unsigned int tmp_tsn = 0, i = 0, current_tsn=0, del_tsn = 0;

    /* tmp->data is a d_chunk struct */
    event_logii (INTERNAL_EVENT_0, "  se_deliver_stream_unreliably_unordered(sid: %u, tsn:%u) ", sid,up_to_tsn);

    while (tmp != NULL) {

        event_log (VERBOSE, "se_deliver_stream_unreliably -> LIST AT THE LOOP START");
        g_list_foreach(se->RecvStreams[sid].unorderedList, &print_element, NULL);

        d_chunk = (delivery_data*)(tmp->data);
        tmp_ssn = d_chunk->stream_sn;
        current_tsn = d_chunk->tsn;
        if (after(current_tsn, up_to_tsn)) break;

        event_logiii (VERBOSE, "se_deliver_stream_unreliably: ssn: %u, tsn: %u, up_to_tsn: %u", tmp_ssn, current_tsn,up_to_tsn);

        if (d_chunk->chunk_flags & SCTP_DATA_BEGIN_SEGMENT) {
            event_log (VERBOSE, "(se_deliver_stream_unreliably -> beginFound !)");
            beginFound = TRUE;
            /*  start = tmp; */
        }
        /* delete until start found */
        if (beginFound == FALSE) {
            event_log (VERBOSE, "(se_deliver_stream_unreliably -> remove from start !)");
            se->queuedBytes -=  d_chunk->data_length;
            free_list_element(d_chunk, NULL);
            se->RecvStreams[sid].unorderedList = g_list_remove(se->RecvStreams[sid].unorderedList,
                                                               g_list_nth_data(se->RecvStreams[sid].unorderedList,0)
);
        } else {
            if (newStarted == TRUE) {
                /* set this once per delivered pdu (and) notification */
                start_tsn = d_chunk->tsn;
                tmp_tsn =   d_chunk->tsn - 1;
                newStarted = FALSE;
            }
            if (d_chunk->chunk_flags & SCTP_DATA_END_SEGMENT) {
                event_log (VERBOSE, "(se_deliver_stream_unreliably -> endFound !)");
                endFound = TRUE;
            }

            if (current_tsn == tmp_tsn + 1){
                event_logi (VERBOSE, "se_deliver_stream_unreliably -> tsnOkay, tmp_tsn=%u !", tmp_tsn);
                tsnOkay = TRUE;
                tmp_tsn++;
            } else {
                /* delete all dangling chunks before current tsn */
                event_log (VERBOSE, "(se_deliver_stream_unreliably -> tsnOkay == FALSE !)");
                tsnOkay = FALSE;
                for (i = 1; i < chunksToDeliver; i++) {
                    if (g_list_nth_data(se->RecvStreams[sid].unorderedList,0)) {
                        d_chunk = (delivery_data*) g_list_nth_data(se->RecvStreams[sid].unorderedList,0);
                        se->queuedBytes -= d_chunk->data_length;
                        /* free data also */
                        free_list_element(d_chunk, NULL);
                        se->RecvStreams[sid].unorderedList = g_list_remove(se->RecvStreams[sid].unorderedList, d_chunk);
                    }
                }
                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; start = NULL; total_length=0;
                chunksToDeliver=1;
                tmp = g_list_first(se->RecvStreams[sid].unorderedList);
                /* start over with the same chunk */
                continue;
            }
            if (beginFound == TRUE && endFound == TRUE && tsnOkay == TRUE) {
                /* deliver data, and set next SSN */
                d_pdu = malloc(sizeof(delivery_pdu));
                if (d_pdu == NULL) {
                    return SCTP_OUT_OF_RESOURCES;
                }
                d_pdu->number_of_chunks = chunksToDeliver;
                d_pdu->read_position = 0;
                d_pdu->read_chunk = 0;
                d_pdu->chunk_position = 0;

                d_pdu->ddata = malloc(chunksToDeliver*sizeof(delivery_data*));
                if (d_pdu->ddata == NULL) {
                    free(d_pdu);
                    return SCTP_OUT_OF_RESOURCES;
                }
                event_logi (VERBOSE, "se_deliver_stream_unreliably -> deliver %u chunk-fragment", chunksToDeliver);
                event_log  (VERBOSE, "se_deliver_stream_unreliably -> LIST BEFORE DATA ARRIVE ");
                g_list_foreach(se->RecvStreams[sid].unorderedList, &print_element, NULL);

                /* get pointers to the first chunks and put them into the pduList */
                for (i = 0; i < chunksToDeliver; i++) {
                    d_pdu->ddata[i] = (delivery_data*)g_list_nth_data(se->RecvStreams[sid].unorderedList, i);
                    total_length += d_pdu->ddata[i]->data_length;
                }
                d_pdu->total_length = total_length;
                d_pdu->fromAddressIndex = d_pdu->ddata[0]->fromAddressIndex;
                del_tsn =  d_pdu->ddata[0]->tsn;
                /* assemble delivery_pdu, and Notify */
                se->RecvStreams[sid].pduList = g_list_append(se->RecvStreams[sid].pduList, d_pdu);

                event_log (VERBOSE, "se_deliver_stream_unreliably -> LIST AFTER APPENDING ");
                g_list_foreach(se->RecvStreams[sid].unorderedList, &print_element, NULL);
                /* remove chunks from the list and return */
                for (i = 0; i < chunksToDeliver; i++) {
                    event_logi (VERBOSE, "remove delivered chunk-fragment %u from queue", i+1);
                    se->RecvStreams[sid].unorderedList = g_list_remove(se->RecvStreams[sid].unorderedList,
                                                         g_list_nth_data(se->RecvStreams[sid].unorderedList,0));
                }
                event_log (VERBOSE, "se_deliver_stream_unreliably -> LIST AFTER REMOVAL ");
                g_list_foreach(se->RecvStreams[sid].unorderedList, &print_element, NULL);

                if (tmp_ssn != 0) {
                    mdi_dataArriveNotif(sid, total_length, tmp_ssn, del_tsn, d_pdu->ddata[0]->protocolId, 0);
                } else {
                    mdi_dataArriveNotif(sid, total_length, tmp_ssn, del_tsn, d_pdu->ddata[0]->protocolId, 1);
                }
                 delivered_ssn = tmp_ssn;

                event_log (VERBOSE, "se_deliver_stream_unreliably -> LIST AFTER DATA ARRIVE ");
                g_list_foreach(se->RecvStreams[sid].unorderedList, &print_element, NULL);

                beginFound = FALSE; endFound = FALSE; newStarted = TRUE; total_length = 0;
                chunksToDeliver = 1;
                tmp = g_list_first(se->RecvStreams[sid].unorderedList);
                continue;

            } else if (beginFound == TRUE && endFound == FALSE && tsnOkay == TRUE) {
                chunksToDeliver++;
            }
        }
        tmp = g_list_next(tmp);
    }   /* end while */
    return delivered_ssn;
}


int se_deliver_unreliably(unsigned int up_to_tsn, SCTP_forward_tsn_chunk* fw_tsn_chk)
{
    int numOfSkippedStreams;
    unsigned short skippedStream, skippedSSN;
    gboolean advancedSSN;
    pr_stream_data* psd;
    int i;

    StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();
    if (se == NULL) {
        error_log (ERROR_MAJOR, "Could not read StreamEngine Instance !");
        return SCTP_MODULE_NOT_FOUND;
    }

    numOfSkippedStreams = (ntohs(fw_tsn_chk->chunk_header.chunk_length) -
                          sizeof(unsigned int) - sizeof(SCTP_chunk_header)) / sizeof(pr_stream_data);

    event_logii (INTERNAL_EVENT_0, "SE: deliver unreliably (fw_tsn=%u, %d skipped streams", up_to_tsn, numOfSkippedStreams);

    if (se->unreliable == TRUE) {
        for (i = 0; i < numOfSkippedStreams; i++)
        {
            psd = (pr_stream_data*) &fw_tsn_chk->variableParams[sizeof(pr_stream_data)*i];
            skippedStream = ntohs(psd->stream_id);
            skippedSSN = ntohs(psd->stream_sn);
            event_logiii (VERBOSE, "delivering dangling messages in stream %d for forward_tsn=%u, SSN=%u",
                        skippedStream, up_to_tsn, skippedSSN);
            /* if unreliable, check if messages can be  delivered */
            se_deliver_stream_unreliably_unordered(se, skippedStream, skippedSSN, up_to_tsn);
            advancedSSN = se_deliver_stream_unreliably_ordered(se, skippedStream, skippedSSN, up_to_tsn);

            /* FIXME in Next Release, TOO !!!!!!!!!!!!!!!!!!*/    
            if (advancedSSN == TRUE) se->RecvStreams[skippedStream].nextSSN = (se->RecvStreams[skippedStream].nextSSN + 1) % 0x10000;
        }
    }
    return SCTP_SUCCESS;
}


int se_getQueuedBytes(void)
{
    StreamEngine* se = (StreamEngine *) mdi_readStreamEngine ();
    if (se == NULL) {
        error_log (ERROR_MAJOR, "Could not read StreamEngine Instance !");
        return -1;
    }
    return (int)se->queuedBytes;
}

