/*
 *  $Id: auxiliary.c,v 1.1 2003/05/16 13:47:49 ajung Exp $
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
 *          Michael.Tuexen@icn.siemens.de
 *          ajung@exp-math.uni-essen.de
 *
 * This module offers functions needed for validation and
 * other purposes.
 *
 */

#include "auxiliary.h"
#include "globals.h"
#include "sctp.h"

#include <stdio.h>

#ifndef HAVE_RANDOM
#define random() rand()
#endif

#define BASE 65521L             /* largest prime smaller than 65536 */
#define NMAX 5552
#define NMIN 16

/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

/* Example of the crc table file */

#define CRC32C(c,d) (c=(c>>8)^crc_c[(c^(d))&0xFF])

unsigned long  crc_c[256] =
{
    0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
    0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
    0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
    0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
    0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
    0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
    0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
    0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
    0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
    0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
    0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
    0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
    0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
    0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
    0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
    0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
    0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
    0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
    0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
    0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
    0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
    0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
    0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
    0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
    0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
    0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
    0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
    0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
    0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
    0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
    0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
    0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
    0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
    0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
    0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
    0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
    0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
    0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
    0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
    0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
    0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
    0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
    0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
    0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
    0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
    0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
    0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
    0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
    0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
    0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
    0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
    0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
    0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
    0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
    0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
    0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
    0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
    0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
    0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
    0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
    0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
    0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
    0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
    0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L,
};

int insert_adler32(unsigned char *buffer, int length);
int insert_crc32(unsigned char *buffer, int length);
int validate_adler32(unsigned char *header_start, int length);
int validate_crc32(unsigned char *buffer, int length);


static int (*insert_checksum) (unsigned char* buffer, int length) = insert_crc32;
static int (*validate_checksum) (unsigned char* buffer, int length) = validate_crc32;


unsigned int adler32(unsigned int adler, const unsigned char *buf, unsigned int len);


int set_checksum_algorithm(int algorithm){
    if (algorithm == SCTP_CHECKSUM_ALGORITHM_CRC32C) {
        insert_checksum =  &insert_crc32;
        validate_checksum =  &validate_crc32;
        return SCTP_SUCCESS;
    } else if (algorithm == SCTP_CHECKSUM_ALGORITHM_ADLER32) {
        insert_checksum =  &insert_adler32;
        validate_checksum =  &validate_adler32;
        return SCTP_SUCCESS;
    } else
        return -1;
}


unsigned char* key_operation(int operation_code)
{
    static unsigned char *secret_key = NULL;
    unsigned int count = 0, tmp;

    if (operation_code == KEY_READ) return secret_key;
    else if (operation_code == KEY_INIT) {
        if (secret_key != NULL) {
            error_log(ERROR_MAJOR, "tried to init secret key, but key already created !");
            return secret_key;
        }
        secret_key = malloc(SECRET_KEYSIZE);
        while (count < SECRET_KEYSIZE){
            /* if you care for security, you need to use a cryptographically secure PRNG */
            tmp = random();
            memcpy(&secret_key[count], &tmp, sizeof(unsigned int));
            count += 4;
        }
    } else {
        error_log(ERROR_MAJOR, "unknown key operation code !");
        return NULL;
    }
    return secret_key;
}

int aux_insert_checksum(unsigned char *buffer, int length)
{
    return ((*insert_checksum)(buffer,length));
}


int insert_adler32(unsigned char *buffer, int length)
{
    SCTP_message *message;
    unsigned int a32;
    /* save crc value from PDU */
    if (length > NMAX || length < NMIN)
        return -1;
    message = (SCTP_message *) buffer;
    message->common_header.checksum = htonl(0L);

    /* now compute the thingie */
    /* FIXME : sanity checks for size etc. */
    a32 = adler32(1L, buffer, length);

    /* and insert it into the message */
    message->common_header.checksum = htonl(a32);

    event_logi(VERBOSE, "DEBUG Validation : Inserting adler32 == %x", a32);

    return 1;
}

unsigned long generate_crc32c(unsigned char *buffer, int length)
{
    unsigned long crc32 = ~0L;
    int i;
    unsigned char byte0, byte1, byte2, byte3, swap;

    for (i = 0; i < length; i++)
    {
      CRC32C(crc32, buffer[i]);
    }
    crc32 =~ crc32;
    /* do the swap */	
    byte0 = crc32 & 0xff;
    byte1 = (crc32>>8) & 0xff;
    byte2 = (crc32>>16) & 0xff;
    byte3 = (crc32>>24) & 0xff;
    swap = byte0; byte0 = byte3; byte3 = swap;
    swap = byte1; byte1 = byte2; byte2 = swap;
    crc32 = ((byte3 << 24)|(byte2 << 16)|(byte1 << 8)| byte0);

    return crc32;

}

int insert_crc32(unsigned char *buffer, int length)
{
    SCTP_message *message;
    unsigned long crc32c;


    /* check packet length */
    if (length > NMAX  || length < NMIN)
      return -1;

    message = (SCTP_message *) buffer;
    message->common_header.checksum = 0L;
    crc32c =  generate_crc32c(buffer, length);
    /* and insert it into the message */
    message->common_header.checksum = htonl(crc32c);

   return 1;
}


int validate_size(unsigned char *header_start, int length)
{
    if ((length % 4) != 0L)
        return 0;
    if (length > NMAX  || length < NMIN)
        return 0;
    return 1;
}



int validate_adler32(unsigned char *header_start, int length)
{
    SCTP_message *message;
    unsigned int old_crc32;
    unsigned int a32;

    /* save crc value from PDU */
    message = (SCTP_message *) header_start;
    old_crc32 = ntohl(message->common_header.checksum);

    event_logi(VVERBOSE, "DEBUG Validation : old adler == %x", old_crc32);

    message->common_header.checksum = htonl(0L);

    /* now compute the thingie */
    a32 = adler32(1L, header_start, length);

    event_logi(VVERBOSE, "DEBUG Validation : my adler32 == %x", a32);
    if (a32 == old_crc32)  return 1;
    return 0;
}

int validate_crc32(unsigned char *buffer, int length)
{
    SCTP_message *message;
    unsigned long original_crc32;
    unsigned long crc32 = ~0L;

    /* check packet length */

    /* save and zero checksum */
    message = (SCTP_message *) buffer;
    original_crc32 = ntohl(message->common_header.checksum);
    event_logi(VVERBOSE, "DEBUG Validation : old crc32c == %x", original_crc32);
    message->common_header.checksum = 0L;
    crc32 = generate_crc32c(buffer, length);
    event_logi(VVERBOSE, "DEBUG Validation : my crc32c == %x", crc32);

    return ((original_crc32 == crc32)? 1 : 0);
}



int validate_datagram(unsigned char *buffer, int length)
{
    /* sanity check for size (min,max, multiple of 32 bits) */
    if (!validate_size(buffer, length))
        return 0;
    if (!(*validate_checksum)(buffer, length))
        return 0;
    /* FIXME :  validation is not yet complete */
    return 1;
}

/** 
 * adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-1996 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 * available, e.g. from  http://www.cdrom.com/pub/infozip/zlib/
 */
unsigned int adler32(unsigned int adler, const unsigned char *buf, unsigned int len)
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    int k;

    if (buf == NULL)
        return 1L;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(buf);
            buf += 16;
            k -= 16;
        }
        if (k != 0)
            do {
                s1 += *buf++;
                s2 += s1;
            }
            while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    return (s2 << 16) | s1;
}
