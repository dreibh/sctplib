#!/bin/sh
./bootstrap && ./configure --enable-static --disable-shared $@ && gmake
