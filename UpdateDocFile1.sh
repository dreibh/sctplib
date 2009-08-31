#!/bin/sh
sed -f UpdateDoc1.sed $1 >/tmp/sedtemp && \
mv /tmp/sedtemp $1
