#!/bin/sh
sed -f UpdateDoc1.sed $1 >/tmp/sedtemp
cp /tmp/sedtemp $1
rm -f /tmp/sedtemp
