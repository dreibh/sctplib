/* Define if the C compiler supports BOOL */
#undef HAVE_BOOL

#undef VERSION

#undef PACKAGE

/* Define if you need the GNU extensions to compile */
#undef _GNU_SOURCE

/* Define if you have the inet_aton function.  */
#undef HAVE_INET_ATON

/* Define if you have the inet_ntop function.  */
#undef HAVE_INET_NTOP

/* Define if you have the inet_pton function.  */
#undef HAVE_INET_PTON

/* Define if you have ipv6 stack.  */
#undef HAVE_IPV6

/* whether sockaddr has a sa_len field */
#undef HAVE_SA_LEN

/* whether sockaddr_in has a sin_len field */
#undef HAVE_SIN_LEN

/* whether sockaddr_in6 has a sin6_scope_id field */
#undef HAVE_SIN6_SCOPE_ID

/* whether we have a <sys/poll.h> include file or need to manually define poll() */
#undef HAVE_SYS_POLL_H

/* whether we have the socklen_t type defined */
#undef HAVE_SOCKLEN_T


/* Define if /proc/net/if_inet6 exists. */
#undef HAVE_PROC_NET_IF_INET6

/* Define if NET_RT_IFLIST exists in sys/socket.h. */
#undef HAVE_NET_RT_IFLIST

/* Define if you have INRIA ipv6 stack.  */
#undef INRIA_IPV6

/* Define if you have KAME project ipv6 stack.  */
#undef KAME

/* Define if you have Linux ipv6 stack.  */
#undef LINUX_IPV6

/* Define if you have NRL ipv6 stack.  */
#undef NRL

/* Define if you have BSDI NRL IPv6 stack. */
#undef BSDI_NRL

#ifdef SUNOS_5
typedef unsigned int u_int32_t; 
typedef unsigned short u_int16_t; 
typedef unsigned short u_int8_t; 
#endif /* SUNOS_5 */


#ifdef HAVE_IPV6
#ifdef KAME
#ifndef INET6
#define INET6
#endif /* INET6 */
#endif /* KAME */
#endif /* HAVE_IPV6 */

#undef  USE_SCTP_WRAPPER

#undef  USE_RFC2292BIS

#undef  HAVE_PKTINFO

#undef  HAVE_NCURSES_H

#undef  HAVE_CURSES_H
