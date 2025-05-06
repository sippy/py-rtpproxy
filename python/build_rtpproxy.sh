#!/bin/sh

set -e

_SELF="`realpath "${0}"`"
MYDIR="`dirname ${_SELF}`"

BDIR="${MYDIR}/../build"

if [ "${CC}" = "clang" ]
then
  export AR="llvm-ar"
  export RANLIB="llvm-ranlib"
fi
if [ "`uname -o`" = "FreeBSD" ]
then
  GMAKE=gmake
else
  GMAKE=make
fi

CFLAGS="-O2 -pipe -flto"
LDFLAGS="-O2 -flto"

cd ${MYDIR}/../openssl
CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./Configure no-shared no-module no-dso \
 no-tests no-apps no-unit-test no-quic no-docs --prefix="${BDIR}" --libdir=lib
make all
make install_sw

CFLAGS="${CFLAGS} -I${BDIR}/include"
LDFLAGS="${LDFLAGS} -L${BDIR}/lib"

cd ${MYDIR}/../libsrtp
CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./configure \
 --prefix="${BDIR}" --enable-static --disable-shared --enable-openssl \
 --with-openssl-dir="${BDIR}"
${GMAKE} libsrtp2.a
${GMAKE} install

cd ${MYDIR}/../rtpproxy

CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./configure \
 --enable-librtpproxy --enable-lto --enable-silent --enable-noinst=no
for dir in libexecinfo libucl libre external/libelperiodic/src libxxHash modules
do
  make -C ${dir} all
done
make -C src librtpproxy.la
