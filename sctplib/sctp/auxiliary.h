/*
 *  $Id: auxiliary.h,v 1.2 2003/07/01 13:58:27 ajung Exp $
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
 * There are two mailinglists available at http://www.sctp.de which should be
 * used for any discussion related to this implementation.
 *
 * Contact: discussion@sctp.de
 *          tuexen@fh-muenster.de
 *          ajung@exp-math.uni-essen.de
 *
 * This module offers functions needed for validation and
 * other purposes.
 *
 */
#ifndef AUXILIARY_H
#define AUXILIARY_H


unsigned char* key_operation(int operation_code);


/**
 * This function performs all necessary checks, that are possible at this
 * layer. Calls subsequent check-functions for each part that may be checked.
 * @param buffer pointer to the start of the received datagram (SCTP PDU)
 * @param length size of the buffer
 * @return 0 if validation failed, 1 if successful, -1 if any error ocurred
 */
int validate_datagram(unsigned char *buffer, int length);

int aux_insert_checksum(unsigned char *buffer, int length);

int set_checksum_algorithm(int algorithm);

#endif

