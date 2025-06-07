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

CFLAGS_opt="-O1 -pipe -flto"
CFLAGS="-O0 -g3 -pipe -flto"
if [ ! -z "${ARCH_CFLAGS}" ]
then
  CFLAGS_opt="${CFLAGS_opt} ${ARCH_CFLAGS}"
  CFLAGS="${CFLAGS} ${ARCH_CFLAGS}"
fi
LDFLAGS_opt="-O2 -flto"
LDFLAGS="-O0 -flto"

if [ -z "${NO_BUILD_OSSL}" ]
then
  cd ${MYDIR}/../openssl
  CFLAGS="${CFLAGS_opt}" LDFLAGS="${LDFLAGS_opt}" ./Configure no-shared no-module no-dso \
   no-tests no-apps no-unit-test no-quic no-docs --prefix="${BDIR}" --libdir=lib \
   ${OPENSSL_CONFIGURE_ARGS}
  make all
  make install_sw
fi

CFLAGS_opt="${CFLAGS_opt} -I${BDIR}/include"
CFLAGS="${CFLAGS} -I${BDIR}/include"
LDFLAGS_opt="${LDFLAGS_opt} -L${BDIR}/lib"
LDFLAGS="${LDFLAGS} -L${BDIR}/lib"

if [ -z "${NO_BUILD_SRTP}" ]
then
  cd ${MYDIR}/../libsrtp
  CFLAGS="${CFLAGS_opt}" LDFLAGS="${LDFLAGS_opt}" ./configure \
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
