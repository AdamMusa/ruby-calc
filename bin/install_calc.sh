#! /bin/sh
# installs calc into /usr/local

set -e
set -x

version=calc-${CALC_VERSION-2.12.7.1}
patch=$PWD/$(dirname $0)/makefile.patch

cd /tmp
wget -nv http://www.isthe.com/chongo/src/calc/${version}.tar.bz2
tar xjf ${version}.tar.bz2
cd $version
patch -t -p0 < $patch
if [ "$CI" = "true" ]; then
    make && sudo make install
else
    make && make check && sudo make install
fi
