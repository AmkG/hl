#!/bin/bash

pushd hl2b/
./autogen.sh
popd
aclocal
autoconf
autoheader
automake -a

exit 0
