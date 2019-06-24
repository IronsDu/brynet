#!/bin/bash
os_name=`uname -o`
if [ "$os_name" = GNU/Linux ]
then
	build-wrapper-linux-x86-64 --out-dir bw-output make
else
	build-wrapper-macosx-x86 --out-dir bw-output make
fi