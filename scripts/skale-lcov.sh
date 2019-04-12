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


# run testeth
cd $THIS_SCRIPT_DIR/build/test
./testeth -- --all --verbosity 4
cd $THIS_SCRIPT_DIR


#lcov --base-directory $SRC_COVERAGE_DIR --directory $SRC_COVERAGE_DIR --zerocounters -q
lcov --base-directory $SRC_COVERAGE_DIR --directory $SRC_COVERAGE_DIR -c -o $DST_COVERAGE_DIR/skale.info
#lcov --remove libbash_test.info "/usr*" -o $DST_COVERAGE_DIR/skale.info # remove output for external libraries

cp $DST_COVERAGE_DIR/skale.info $DST_COVERAGE_DIR/skale.info.just_gathered

rm -f $DST_COVERAGE_DIR/skale.info.filtered
lcov --remove $DST_COVERAGE_DIR/skale.info \
    '/usr/include/*' '/usr/lib/*' \
    '*.hunter*' '*boost*' '*utils/json_spirit*' \
    '*/cpp-ethereum/SkaleDeps/*' '*/cpp-ethereum/build/deps/*' \
    '*test/tools/*' '*test/unittests/*' \
    -o $DST_COVERAGE_DIR/skale.info.filtered

rm -rf $DST_COVERAGE_DIR/skale-lcov
genhtml -o $DST_COVERAGE_DIR/skale-lcov -t "SKALE COVERAGE" --num-spaces 4 $DST_COVERAGE_DIR/skale.info.filtered &> $DST_COVERAGE_DIR/genhtml.output.txt
export STR_LINES=`cat $DST_COVERAGE_DIR/genhtml.output.txt | grep "lines......:" | cut -c 16-200`
export STR_FUNCTIONS=`cat $DST_COVERAGE_DIR/genhtml.output.txt | grep "functions..:" | cut -c 16-200`
echo $STR_LINES
echo $STR_FUNCTIONS
