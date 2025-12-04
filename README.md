<h1 align="center">
 SCTPLIB<br />
 <span style="font-size: 75%;">A User-Space SCTP Implementation</span><br />
 <a href="https://www.nntb.no/~dreibh/sctplib/">
  <img alt="DynMHS Logo" src="https://www.nntb.no/~dreibh/sctp/images/SCTPProject.svg" width="25%" /><br />
  <span style="font-size: 75%;">https://www.nntb.no/~dreibh/sctplib</span>
 </a>
</h1>


# 💡 What is SCTPLIB?

SCTPLIB is a user-space implementation of the Stream Control Transmission Protocol, as per [RFC&nbsp;4960](https://www.rfc-editor.org/rfc/rfc4960.html).

## Copyright

* Copyright (C) 2000 by Siemens AG, Munich, Germany.
* Copyright (C) 2001-2004 Andreas Jungmaier
* Copyright (C) 2004-2026 Thomas Dreibholz

Realized in co-operation between
Siemens AG and
the [Computer Networking Technolog Group](https://web.archive.org/web/20190409135141/https://www.tdr.wiwi.uni-due.de/en/team/)
of the [Institute for Experimental Mathematics](https://web.archive.org/web/20120723021117/http://web.iem.uni-due.de/portrait/)
at the [University of Duisburg-Essen](https://www.uni-due.de/) in Essen, Germany.

## Acknowledgement

This work was partially funded by the Bundesministerium für Bildung und
Forschung&nbsp;(BMBF) of the Federal Republic of Germany (Förderkennzeichen 01AK045).
The authors alone are responsible for the contents.


# 📚 Details

It contains a number of files and directories that are shortly described hereafter:

* [`AUTHORS`](AUTHORS): People who have produced this code.
* [`COPYING`](COPYING): The license to be applied for using/compiling/distributing this code.
* [`INSTALL`](INSTALL): Basic installation notes.
* [`README.md`](README.md): This file.
* [`TODO`](TODO): A more or less complete list of missing features/known problems, etc.
* [`sctplib`](sctplib): A directory containig the files belonging to the SCTP implementation.
* [`sctplib/sctp`](sctplib/sctp): contains the files for the actual library, which will be named libsctp.a. Also contains the necessary include file [sctp.h](sctplib/sctp/sctp.h).
* [`sctplib/manual`](sctplib/manual): A subdirectory containing some tex files and a PDF manual, describing general implementation features, the API, and the example programs. Now also in PostScript. PLEASE NOTE: THIS DOCUMENTATION IS SLIGHTLY OUT OF DATE! IF YOU DO NOT LIKE THIS, FEEL FREE TO CONTRIBUTE UPDATES. But promised: we will be working on that ...
* [`sctplib/docs`](sctplib/docs): This subdirectory will contain documentation after the build process, that has been automatically derived from the source code.

Happy SCTPing! :-)


# 💾 Build from Sources

DynMHS is released under the [GNU General Public Licence&nbsp;(GPL)](https://www.gnu.org/licenses/gpl-3.0.en.html#license-text).

Please use the issue tracker at [https://github.com/dreibh/sctplib/issues](https://github.com/dreibh/sctplib/issues) to report bugs and issues!

## Development Version

The Git repository of the DynMHS sources can be found at [https://github.com/dreibh/sctplib](https://github.com/dreibh/sctplib):

```bash
git clone https://github.com/dreibh/sctplib
cd sctplib
sudo ci/get-dependencies --install
./configure
make
```

Note: The script [`ci/get-dependencies`](https://github.com/dreibh/sctplib/blob/master/ci/get-dependencies) automatically  installs the build dependencies under Debian/Ubuntu Linux, Fedora Linux, and FreeBSD. For manual handling of the build dependencies, see the packaging configuration in [`debian/control`](https://github.com/dreibh/sctplib/blob/master/debian/control) (Debian/Ubuntu Linux), [`sctplib.spec`](https://github.com/dreibh/sctplib/blob/master/rpm/sctplib.spec) (Fedora Linux), and [`Makefile`](https://github.com/dreibh/sctplib/blob/master/freebsd/sctplib/Makefile) FreeBSD.

Contributions:

* Issue tracker: [https://github.com/dreibh/sctplib/issues](https://github.com/dreibh/sctplib/issues).
  Please submit bug reports, issues, questions, etc. in the issue tracker!

* Pull Requests for DynMHS: [https://github.com/dreibh/sctplib/pulls](https://github.com/dreibh/sctplib/pulls).
  Your contributions to DynMHS are always welcome!

* CI build tests of DynMHS: [https://github.com/dreibh/sctplib/actions](https://github.com/dreibh/sctplib/actions).

## Release Versions

See [https://www.nntb.no/~dreibh/sctplib/#current-stable-release](https://www.nntb.no/~dreibh/sctplib/#current-stable-release) for the release packages!


# 🔗 Useful Links

* [HiPerConTracer – High-Performance Connectivity Tracer](https://www.nntb.no/~dreibh/hipercontracer/)
* [NetPerfMeter – A TCP/MPTCP/UDP/SCTP/DCCP Network Performance Meter Tool](https://www.nntb.no/~dreibh/netperfmeter/)
* [SubNetCalc – An IPv4/IPv6 Subnet Calculator](https://www.nntb.no/~dreibh/sctplib/)
* [System-Tools – Tools for Basic System Management](https://www.nntb.no/~dreibh/system-tools/)
* [Virtual Machine Image Builder and System Installation Scripts](https://www.nntb.no/~dreibh/vmimage-builder-scripts/)
* [Wireshark](https://www.wireshark.org/)
