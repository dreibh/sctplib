/***************************************************************************
                                    test_tool.c
                             --------------------
    begin                : Fri Mar  2 09:43:00 CET 2001
    copyright            : (C) 2001 by Andreas Lang
    email                : anla@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include "sctptest.h"
#include <sctp.h>

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008

/**
 * Callback function used to terminate the programm. It is invoked when
 * the user hits the "return" key after the end of the script file was reached.
 */
void exitCallback(int fd, short int revents, short int* gotEvents, void * dummy)
{
    // clear stdin buffer
    while(getchar() != 10);

    exit(0);
}


/**
 * Main function
 * Reads the script file name from the command line, checks if there are any errors
 * in the script, and if there are none, the script is started. After the script has
 * been completely executed, the program stays in the event loop (thus listens for
 * arriving chunks etc.) until the user hits the "return" key.
 */
int main(int argc, char *argv[])
{
    unsigned int numOfErrors = 0;

    // check if there is exactly one command line parameter
    if (argc != 2) {
        fprintf(stderr, "SCTPTest by Andreas Lang\n");
        fprintf(stderr, "Usage: sctptest <scriptfile>\n");
        exit(1);
    }

    sctp_initLibrary();

    // check script for errors
    if ((numOfErrors = sctptest_start(argv[1], CHECK_SCRIPT)) != 0) {
        fprintf(stderr, "\n%u error(s) in script file!\n", numOfErrors);
        exit(1);
    }

    // run script
    sctptest_start(argv[1], RUN_SCRIPT);

    fprintf(stderr, "\nReached end of script file. Press RETURN to exit.\n");
    sctp_registerUserCallback(fileno(stdin), &exitCallback, NULL, POLLPRI|POLLIN);

    while (sctp_eventLoop() >= 0);

    // this will never be reached
    return 0;
}

