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

CFLAGS="-O0 -g3 -pipe -flto"
LDFLAGS="-O0 -g3 -flto"

if [ -z "${NO_BUILD_OSSL}" ]
then
  cd ${MYDIR}/../openssl
  CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./Configure no-shared no-module no-dso \
   no-tests no-apps no-unit-test no-quic no-docs --prefix="${BDIR}" --libdir=lib
  make all
  make install_sw
fi

CFLAGS="${CFLAGS} -I${BDIR}/include"
LDFLAGS="${LDFLAGS} -L${BDIR}/lib"

if [ -z "${NO_BUILD_SRTP}" ]
then
  cd ${MYDIR}/../libsrtp
  CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./configure \
   --prefix="${BDIR}" --enable-static --disable-shared --enable-openssl \
   --with-openssl-dir="${BDIR}"
  ${GMAKE} libsrtp2.a
  ${GMAKE} install
fi

cd ${MYDIR}/../rtpproxy

CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}" ./configure --enable-static-crypto \
 --enable-librtpproxy --enable-lto --enable-noinst=no
for dir in libexecinfo libucl libre external/libelperiodic/src libxxHash modules
do
  make -C ${dir} all
done
make -C src librtpproxy.la
