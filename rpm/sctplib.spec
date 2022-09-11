Name: sctplib
Version: 1.0.29
Release: 1
Summary: User-space implementation of the SCTP protocol RFC 4960
License: LGPL-3+
Group: Applications/Internet
URL: https://www.uni-due.de/~be0001/sctplib/
Source: https://www.uni-due.de/~be0001/sctplib/download/%{name}-%{version}.tar.gz

AutoReqProv: on
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: ghostscript
BuildRequires: glib2-devel
BuildRequires: libtool
BuildRequires: texlive-collection-fontsrecommended
BuildRequires: texlive-collection-latex
BuildRequires: texlive-collection-latexextra
BuildRoot: %{_tmppath}/%{name}-%{version}-build

# TEST ONLY:
# define _unpackaged_files_terminate_build 0

%description
The sctplib library is a fairly complete prototype implementation of the
Stream Control Transmission Protocol (SCTP), a message-oriented reliable
transport protocol that supports multi-homing, and multiple message streams
multiplexed within an SCTP connection (also named association). SCTP is
described in RFC 4960. This implementation is the product of a cooperation
between Siemens AG (ICN), Munich, Germany and the Computer Networking
Technology Group at the IEM of the University of Essen, Germany.
The API of the library was modeled after Section 10 of RFC 4960, and most
parameters and functions should be self-explanatory to the user familiar with
this document. In addition to these interface functions between an Upper
Layer Protocol (ULP) and an SCTP instance, the library also provides a number
of helper functions that can be used to manage callback function routines to
make them execute at a certain point of time (i.e. timer-based) or open and
bind  UDP sockets on a configurable port, which may then be used for an
asynchronous interprocess communication.

%prep
%setup -q

%build
autoreconf -i

%configure --prefix=/usr --enable-static --enable-shared --enable-sctp-over-udp
make %{?_smp_mflags}

%install
make DESTDIR=%{buildroot} install


%package libsctplib
Summary: User-space implementation of the SCTP protocol RFC 4960
Group: System Environment/Libraries

%description libsctplib
This package contains the shared library for SCTPLIB.
The SCTPLIB library is a fairly complete prototype implementation of the
Stream Control Transmission Protocol (SCTP), a message-oriented reliable
transport protocol that supports multi-homing, and multiple message streams
multiplexed within an SCTP connection (also named association). SCTP is
described in RFC 4960. This implementation is the product of a cooperation
between Siemens AG (ICN), Munich, Germany and the Computer Networking
Technology Group at the IEM of the University of Essen, Germany.
The API of the library was modeled after Section 10 of RFC 4960, and most
parameters and functions should be self-explanatory to the user familiar with
this document. In addition to these interface functions between an Upper
Layer Protocol (ULP) and an SCTP instance, the library also provides a number
of helper functions that can be used to manage callback function routines to
make them execute at a certain point of time (i.e. timer-based) or open and
bind  UDP sockets on a configurable port, which may then be used for an
asynchronous interprocess communication.

%files libsctplib
%{_libdir}/libsctplib.so.*


%package libsctplib-devel
Summary: Headers and libraries of the user-space SCTP implementation SCTPLIB
Group: Development/Libraries
Requires: %{name}-libsctplib = %{version}-%{release}

%description libsctplib-devel
This package contains development files for SCTPLIB.
The SCTPLIB library is a fairly complete prototype implementation of the
Stream Control Transmission Protocol (SCTP), a message-oriented reliable
transport protocol that supports multi-homing, and multiple message streams
multiplexed within an SCTP connection (also named association). SCTP is
described in RFC 4960. This implementation is the product of a cooperation
between Siemens AG (ICN), Munich, Germany and the Computer Networking
Technology Group at the IEM of the University of Essen, Germany.
The API of the library was modeled after Section 10 of RFC 4960, and most
parameters and functions should be self-explanatory to the user familiar with
this document. In addition to these interface functions between an Upper
Layer Protocol (ULP) and an SCTP instance, the library also provides a number
of helper functions that can be used to manage callback function routines to
make them execute at a certain point of time (i.e. timer-based) or open and
bind  UDP sockets on a configurable port, which may then be used for an
asynchronous interprocess communication.

%files libsctplib-devel
%{_includedir}/sctp.h
%{_libdir}/libsctplib*.a
%{_libdir}/libsctplib*.la
%{_libdir}/libsctplib*.so


%package docs
Summary: Documentation of the user-space SCTP implementation SCTPLIB
Group: System Environment/Libraries
BuildArch: noarch
Requires: %{name}-libsctplib = %{version}-%{release}

%description docs
This package contains documentation files for SCTPLIB.
The SCTPLIB library is a fairly complete prototype implementation of the
Stream Control Transmission Protocol (SCTP), a message-oriented reliable
transport protocol that supports multi-homing, and multiple message streams
multiplexed within an SCTP connection (also named association). SCTP is
described in RFC 4960. This implementation is the product of a cooperation
between Siemens AG (ICN), Munich, Germany and the Computer Networking
Technology Group at the IEM of the University of Essen, Germany.

%files docs
%doc sctplib/manual/*.pdf


%changelog
* Sun Sep 11 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.29
- New upstream release.
* Thu Feb 17 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.28
- New upstream release.
* Wed Feb 16 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.27
- New upstream release.
* Fri Nov 13 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.26
- New upstream release.
* Fri Feb 07 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.25
- New upstream release.
* Wed Aug 14 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.24
- New upstream release.
* Wed Aug 07 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.23
- New upstream release.
* Tue Aug 06 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.22
- New upstream release.
* Fri Jun 14 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.0.22
- New upstream release.
