#!/bin/bash
#
# see https://wiki.documentfoundation.org/Development/Lcov
#
# NOTICE: to build cpp-ethereum you need to specify COVERAGE=ON defined for cmake:
# reset && cmake -DCMAKE_BUILD_TYPE=Debug -DSKALED_HATE_WARNINGS=OFF -DCOVERAGE=ON .. && make -j4
#
export THIS_SCRIPT_DIR=`pwd`
export SRC_COVERAGE_DIR=`pwd`
export DST_COVERAGE_DIR=/tmp
echo "geninfo_auto_base = 1" > ~/.lcovrc
cat ~/.lcovrc
rm -f $DST_COVERAGE_DIR/skale.info
rm -f $DST_COVERAGE_DIR/skale.info.just_gathered
rm -f $DST_COVERAGE_DIR/skale.info.filtered
rm -f $DST_COVERAGE_DIR/genhtml.output.txt
#lcov --zerocounters --directory $SRC_COVERAGE_DIR
lcov --base-directory $SRC_COVERAGE_DIR --directory $SRC_COVERAGE_DIR --zerocounters -q
rm -rf $DST_COVERAGE_DIR/skale-lcov
mkdir -p $DST_COVERAGE_DIR/skale-lcov
