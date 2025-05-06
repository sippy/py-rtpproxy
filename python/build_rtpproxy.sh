#!/bin/sh

set -e

_SELF="`realpath "${0}"`"
MYDIR="`dirname ${_SELF}`"

BDIR="${MYDIR}/../build"

cd ${MYDIR}/../openssl
CFLAGS="-O2 -pipe -flto" LDFLAGS="-O2 -flto" ./Configure no-shared no-module no-dso \
 no-tests no-apps no-unit-test no-quic no-docs --prefix="${BDIR}"
make all
make install_sw

cd ${MYDIR}/../rtpproxy

CFLAGS="-I${BDIR}/include" LDFLAGS="-L${BDIR}/lib" ./configure \
 --enable-librtpproxy --enable-lto --enable-silent --enable-noinst=no
for dir in libexecinfo libucl libre external/libelperiodic/src libxxHash modules
do
  make -C ${dir} all
done
make -C src librtpproxy.la
