###### kdevelop will overwrite this part!!! (begin)##########

EXTRA_DIST = combined_server.c daytime_server.c discard_server.c echo_server.c echo_tool.c \
            terminal.c parser.c script1 script2 sctptest.h test_tool.c testengine.c main.c mini-ulp.c mini-ulp.h \
            sctp_wrapper.h sctp_wrapper.c monitor.c chat.c echo_monitor.c localcom.c chargen_server.c Makefile.nmake
           # traffic_tool1.c traffic_tool2.c  sctp_params.h


####### kdevelop will overwrite this part!!! (end)############
## bin_PROGRAMS = combined_server daytime_server discard_server echo_server echo_tool terminal test_tool mulp @cursesprogs_BIN@

#noinst_PROGRAMS = combined_server daytime_server discard_server echo_server echo_tool terminal test_tool mulp localcom @cursesprogs_BIN@ traffic_tool2 traffic_tool1 chargen_server

noinst_PROGRAMS = combined_server daytime_server discard_server echo_server echo_tool terminal test_tool mulp localcom @cursesprogs_BIN@ chargen_server
EXTRA_PROGRAMS = monitor chat echo_monitor 

combined_server_SOURCES = combined_server.c sctp_wrapper.c
combined_server_LDADD =  ../sctp/libsctplib.la @glib_LIBS@

daytime_server_SOURCES = daytime_server.c sctp_wrapper.c
daytime_server_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

discard_server_SOURCES = discard_server.c sctp_wrapper.c
discard_server_LDADD = ../sctp/libsctplib.la  @glib_LIBS@

chargen_server_SOURCES = chargen_server.c sctp_wrapper.c
chargen_server_LDADD = ../sctp/libsctplib.la  @glib_LIBS@

echo_server_SOURCES = echo_server.c sctp_wrapper.c
echo_server_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

echo_tool_SOURCES = echo_tool.c sctp_wrapper.c
echo_tool_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

terminal_SOURCES = terminal.c sctp_wrapper.c
terminal_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

test_tool_SOURCES = parser.c test_tool.c testengine.c
test_tool_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

mulp_SOURCES = main.c mini-ulp.c
mulp_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

localcom_SOURCES = localcom.c sctp_wrapper.c
localcom_LDADD =  ../sctp/libsctplib.la  @glib_LIBS@

#traffic_tool1_SOURCES = traffic_tool1.c sctp_wrapper.c
#traffic_tool1_LDADD =  ../sctp/libsctplib.la  -lm @glib_LIBS@

#traffic_tool2_SOURCES = traffic_tool2.c sctp_wrapper.c
#traffic_tool2_LDADD =  ../sctp/libsctplib.la  -lm @glib_LIBS@

monitor_SOURCES = monitor.c
monitor_LDADD = @curses_LIBS@ ../sctp/libsctplib.la  @glib_LIBS@

echo_monitor_SOURCES = echo_monitor.c
echo_monitor_LDADD = @curses_LIBS@ ../sctp/libsctplib.la  @glib_LIBS@

chat_SOURCES = chat.c
chat_LDADD = @curses_LIBS@ ../sctp/libsctplib.la  @glib_LIBS@


# set the include path found by configure
# INCLUDES= $(all_includes)

INCLUDES = -I$(srcdir)/../sctp
# the library search path.
#sctplib_LDFLAGS = $(all_libraries) 
#LDFLAGS :=$(LDFLAGS) -L../sctp -lsctp `gtk-config --libs`

                                                                                
