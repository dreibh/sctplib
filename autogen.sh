#!/bin/sh
aclocal && autoconf && autoheader && automake && ./configure $@ && gmake
