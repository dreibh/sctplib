#!/bin/sh
./bootstrap && ./configure --enable-static --disable-shared --enable-maintainer-mode $@ && make
