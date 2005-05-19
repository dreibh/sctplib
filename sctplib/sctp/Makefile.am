####### kdevelop will overwrite this part!!! (begin)##########

# noinst_LIBRARIES = libsctplib.a

## libsctplib_a_SOURCES = timer_list.c streamengine.c sbundling.c reltransfer.c recvctrl.c rbundling.c pathmanagement.c md5.c globals.c flowcontrol.c errorhandler.c distribution.c chunkHandler.c auxiliary.c adaptation.c SCTP-control.c


####### kdevelop will overwrite this part!!! (end)############
# Build a libtool library, libsctp.la for installation in libdir.

lib_LTLIBRARIES        = libsctplib.la
libsctplib_la_SOURCES  = adaptation.c adaptation.h \
                         auxiliary.c auxiliary.h  \
                         sbundling.c rbundling.c \
                         bundling.h \
                         chunkHandler.c chunkHandler.h \
                         distribution.c distribution.h \
                         errorhandler.c errorhandler.h \
                         flowcontrol.c flowcontrol.h \
                         globals.c globals.h \
                         md5.c md5.h \
                         messages.h \
                         pathmanagement.c pathmanagement.h \
                         rbundling.c \
                         recvctrl.c recvctrl.h \
                         reltransfer.c reltransfer.h \
                         sbundling.c  \
                         streamengine.c streamengine.h \
                         timer_list.c timer_list.h

include_HEADERS        = sctp.h
libsctplib_la_LIBADD   = @glib_LIBS@
libsctplib_la_LDFLAGS  = -version-info 1:3:0
