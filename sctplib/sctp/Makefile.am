####### kdevelop will overwrite this part!!! (begin)##########

# noinst_LIBRARIES = libsctp.a

## libsctp_a_SOURCES = timer_list.c streamengine.c sbundling.c reltransfer.c recvctrl.c rbundling.c pathmanagement.c md5.c globals.c flowcontrol.c errorhandler.c distribution.c chunkHandler.c auxiliary.c adaptation.c SCTP-control.c


EXTRA_DIST = SCTP-control.c SCTP-control.h adaptation.c adaptation.h auxiliary.c auxiliary.h bundling.h chunkHandler.c chunkHandler.h distribution.c distribution.h errorhandler.c errorhandler.h flowcontrol.c flowcontrol.h globals.c globals.h md5.c md5.h messages.h pathmanagement.c pathmanagement.h rbundling.c recvctrl.c recvctrl.h reltransfer.c reltransfer.h sbundling.c sctp.h streamengine.c streamengine.h timer_list.c timer_list.h 

####### kdevelop will overwrite this part!!! (end)############
# Build a libtool library, libsctp.la for installation in libdir.

lib_LTLIBRARIES = libsctp.la
libsctp_la_SOURCES = sbundling.c rbundling.c timer_list.c streamengine.c reltransfer.c recvctrl.c pathmanagement.c md5.c globals.c flowcontrol.c errorhandler.c distribution.c chunkHandler.c auxiliary.c adaptation.c SCTP-control.c sctp.h

include_HEADERS = sctp.h

#libsctp_la_LDFLAGS = -version-info 3:12:1
libsctp_la_LDFLAGS = -version-info 2:0:1
