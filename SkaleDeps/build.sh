#!/bin/bash

#env_clear_all() {
#	for i in $(env | awk -F"=" '{print $1}') ; do
#	unset $i ; done
#}

env_save_original() {
	export > ./saved_environment_on_startup.txt
}

env_restore_original() {
	#env_clear_all
	source ./saved_environment_on_startup.txt
}

env_save_original

# colors/basic
COLOR_RESET='\033[0m' # No Color
COLOR_BLACK='\033[0;30m'
COLOR_DARK_GRAY='\033[1;30m'
COLOR_BLUE='\033[0;34m'
COLOR_LIGHT_BLUE='\033[1;34m'
COLOR_GREEN='\033[0;32m'
COLOR_LIGHT_GREEN='\033[1;32m'
COLOR_CYAN='\033[0;36m'
COLOR_LIGHT_CYAN='\033[1;36m'
COLOR_RED='\033[0;31m'
COLOR_LIGHT_RED='\033[1;31m'
COLOR_MAGENTA='\033[0;35m'
COLOR_LIGHT_MAGENTA='\033[1;35m'
COLOR_BROWN='\033[0;33m'
COLOR_YELLOW='\033[1;33m'
COLOR_LIGHT_GRAY='\033[0;37m'
COLOR_WHITE='\033[1;37m'
# colors/variables
COLOR_ERROR="${COLOR_RED}"
COLOR_WARN="${COLOR_YELLOW}"
COLOR_ATTENTION="${COLOR_LIGHT_CYAN}"
COLOR_SUCCESS="${COLOR_GREEN}"
COLOR_INFO="${COLOR_BLUE}"
COLOR_NOTICE="${COLOR_MAGENTA}"
COLOR_DOTS="${COLOR_DARK_GRAY}"
COLOR_SEPARATOR="${COLOR_LIGHT_MAGENTA}"
COLOR_VAR_NAME="${COLOR_BLUE}"
COLOR_VAR_DESC="${COLOR_BROWN}"
COLOR_VAR_VAL="${COLOR_LIGHT_GRAY}"
COLOR_PROJECT_NAME="${COLOR_LIGHT_BLUE}"

# detect system name and number of CPU cores
export UNIX_SYSTEM_NAME=`uname -s`
export NUMBER_OF_CPU_CORES=1
if [ "$UNIX_SYSTEM_NAME" = "Linux" ];
then
	export NUMBER_OF_CPU_CORES=`grep -c ^processor /proc/cpuinfo`
	export READLINK=readlink
	export SO_EXT=so
fi
if [ "$UNIX_SYSTEM_NAME" = "Darwin" ];
then
	#export NUMBER_OF_CPU_CORES=`system_profiler | awk '/Number Of CPUs/{print $4}{next;}'`
	export NUMBER_OF_CPU_CORES=`sysctl -n hw.ncpu`
	# required -> brew install coreutils
	export READLINK=/usr/local/bin/greadlink
	export SO_EXT=dylib
fi

# detect working directories, change if needed
WORKING_DIR_OLD=`pwd`
WORKING_DIR_NEW="$(dirname "$0")"
WORKING_DIR_OLD=`$READLINK -f $WORKING_DIR_OLD`
WORKING_DIR_NEW=`$READLINK -f $WORKING_DIR_NEW`
cd $WORKING_DIR_NEW

#
# MUST HAVE: make, git, svn, nasm, yasm, wget, cmake, ccmake, libtool, libtool_bin, autogen, automake, autopoint, gperf, awk (mawk or gawk), sed, shtool, texinfo, pkg-config
#
#

#
# move values of command line arguments into variables
#
argc=$#
argv=($@)
for (( j=0; j<argc; j++ )); do
	#echo ${argv[j]}
	PARAM=`echo ${argv[j]} | awk -F= '{print $1}'`
	VALUE=`echo ${argv[j]} | awk -F= '{print $2}'`
	#echo ${PARAM}
	#echo ${VALUE}
	export ${PARAM}=${VALUE}
done
#
#
#

simple_find_tool_program () { # program_name, var_name_to_export_full_path, is_optional("yes" or "no")
	#echo $1
	#echo $2
	TMP_CMD="export $2=`which $1`"
	$TMP_CMD
	TMP_CMD="echo ${!2}"
	#echo "TMP_CMD is" $TMP_CMD
	TMP_VAL=`$TMP_CMD`
	#echo "TMP_VAL is" $TMP_VAL
	if [ "$TMP_VAL" = "" ];
	then
		TMP_CMD="export $2=/usr/local/bin/$1"
		$TMP_CMD
		TMP_CMD="echo ${!2}"
		TMP_VAL=`$TMP_CMD`
		if [ -f "$TMP_VAL" ];
		then
			#echo -e "${COLOR_SUCCESS}SUCCESS: $2 found as $TMP_VAL" "${COLOR_RESET}"
			return 0
		fi
		#TMP_CMD="export $2=/opt/local/bin/$1"
		#$TMP_CMD
		#TMP_CMD="echo ${!2}"
		#TMP_VAL=`$TMP_CMD`
		#if [ -f "$TMP_VAL" ];
		#then
		#	#echo -e "${COLOR_SUCCESS}SUCCESS: $2 found as $TMP_VAL" "${COLOR_RESET}"
		#	return 0
		#fi
	fi
	if [ -f "$TMP_VAL" ];
	then
		#echo -e "${COLOR_SUCCESS}SUCCESS: $2 found as $TMP_VAL" "${COLOR_RESET}"
		return 0
	fi
	if [ "$3" = "yes" ];
	then
		return 0
	fi
	echo -e "${COLOR_ERROR}error: $2 tool was not found by deps build script${COLOR_RESET}"
	cd $WORKING_DIR_OLD
	env_restore_original
	exit -1
}

simple_find_tool_program "make" "MAKE" "no"
simple_find_tool_program "makeinfo" "MAKEINFO" "no"
simple_find_tool_program "cmake" "CMAKE" "no"
simple_find_tool_program "ccmake" "CCMAKE" "yes"
#simple_find_tool_program "scons" "SCONS" "yes"
simple_find_tool_program "wget" "WGET" "no"
simple_find_tool_program "autoconf" "AUTOCONF" "no"
simple_find_tool_program "autogen" "AUTOGEN" "yes"
simple_find_tool_program "automake" "AUTOMAKE" "yes"
simple_find_tool_program "m4" "M4" "yes"
#simple_find_tool_program "libtool" "LIBTOOL" "no"
#simple_find_tool_program "shtool" "SHTOOL" "no"
simple_find_tool_program "pkg-config" "PKG_CONFIG" "yes"
simple_find_tool_program "sed" "SED" "no"
simple_find_tool_program "awk" "AWK" "no"
#simple_find_tool_program "yasm" "YASM" "no"
#simple_find_tool_program "nasm" "NASM" "no"

echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}PREPARE BUILD${COLOR_SEPARATOR} ================================${COLOR_RESET}"

if [ -z "${ARCH}" ];
then
	# if we don't have explicit ARCH=something from command line arguments
	ARCH="x86_or_x64"
else
	if [ "$ARCH" = "arm" ];
	then
		ARCH="arm"
	else
		ARCH="x86_or_x64"
	fi
fi
if [ "$ARM" = "1" ];
then
	# if we have explicit ARM=1 from command line arguments
	ARCH="arm"
fi
#
TOP_CMAKE_BUILD_TYPE="Release"
if [ "$DEBUG" = "1" ];
then
	DEBUG=1
	TOP_CMAKE_BUILD_TYPE="Debug"
	DEBUG_D="d"
	CONF_DEBUG_OPTIONS="--enable-debug"
else
	DEBUG=0
	DEBUG_D=""
	CONF_DEBUG_OPTIONS=""
fi
#
if [ -z "${USE_LLVM}" ];
then
	USE_LLVM="0"
fi
if [ "$ARCH" = "arm" ];
then
	USE_LLVM="0"
fi
if [ "$USE_LLVM" != "0" ];
then
	USE_LLVM="1"
fi

if [ -z "${WITH_GTEST}" ];
then
	WITH_GTEST=0
else
	WITH_GTEST=1
fi

export CXXFLAGS="$CXXFLAGS -fPIC"
WITH_ZLIB="yes"
WITH_OPENSSL="yes"
WITH_CURL="yes"
WITH_LZMA="no"
WITH_SSH="no"
WITH_SDL="no"
WITH_SDL_TTF="no"
WITH_EV="no"
WITH_EVENT="no"
WITH_UV="yes"
WITH_LWS="yes"
WITH_V8="no"
WITH_SOURCEY="no"
# notice: WITH_EV and WITH_EVENT should not be used at a same time
WITH_BOOST="no"
WITH_PUPNP="no"
WITH_ARGTABLE2="yes"
#
# notice: nettle and gnutls are needed for microhttpd on ubuntu 18.04
# sudo apt-get install nettle-dev gnutls-dev
#
WITH_NETTLE="no"
WITH_TASN1="no"
WITH_GNU_TLS="no"
#
WITH_GPGERROR="no"
WITH_GCRYPT="no"
WITH_MICRO_HTTP_D="yes"
WITH_JSONCPP="yes"
WITH_JSONRPCCPP="yes"
WITH_CRYPTOPP="yes"

if [ -z "${PARALLEL_COUNT}" ];
then
	PARALLEL_COUNT=$NUMBER_OF_CPU_CORES
fi
if [[ $PARALLEL_COUNT -gt 1 ]];
then
	PARALLEL_MAKE_OPTIONS=" -j $PARALLEL_COUNT "
else
	PARALLEL_MAKE_OPTIONS=""
fi

export CUSTOM_BUILD_ROOT=$PWD
export INSTALL_ROOT_RELATIVE="$CUSTOM_BUILD_ROOT/deps_inst/$ARCH"
mkdir -p "$INSTALL_ROOT_RELATIVE"
export INSTALL_ROOT=`$READLINK -f $INSTALL_ROOT_RELATIVE`
export SOURCES_ROOT=`$READLINK -f $CUSTOM_BUILD_ROOT`
export PREDOWNLOADED_ROOT=`$READLINK -f $CUSTOM_BUILD_ROOT/pre_downloaded`
export LIBRARIES_ROOT=$INSTALL_ROOT/lib
#export DYLD_LIBRARY_PATH=`$READLINK -f $INSTALL_ROOT/lib`
mkdir -p $SOURCES_ROOT
mkdir -p $INSTALL_ROOT
mkdir -p $INSTALL_ROOT/share
mkdir -p $INSTALL_ROOT/share/pkgconfig

# we need this custom prefix bin dir in PATH for tools like gpg-error-config which we build here
export PATH=$PATH:$INSTALL_ROOT/bin

export TOOLCHAINS_PATH=/usr/local/toolchains
export TOOLCHAINS_DOWNLOADED_PATH=$TOOLCHAINS_PATH/downloads

export ARM_TOOLCHAIN_NAME=gcc7.2-arm
export ARM_GCC_VER=7.2.0

#export ARM_TOOLCHAIN_NAME=gcc4.8-arm
#export ARM_GCC_VER=4.8.4

export ARM_TOOLCHAIN_PATH=$TOOLCHAINS_PATH/$ARM_TOOLCHAIN_NAME

export ADDITIONAL_INCLUDES="-I$INSTALL_ROOT/include"
export ADDITIONAL_LIBRARIES="-L$INSTALL_ROOT/lib"
export TOOLCHAIN=no

export CFLAGS=" -fPIC ${CFLAGS}"

if [ "$ARCH" = "x86_or_x64" ];
then
	export CMAKE_CROSSCOMPILING_OPTS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
	#export MAKE_CROSSCOMPILING_OPTS=""
	export CONF_CROSSCOMPILING_OPTS_GENERIC=""
	export CONF_CROSSCOMPILING_OPTS_VORBIS=""
	export CONF_CROSSCOMPILING_OPTS_CURL=""
	export CONF_CROSSCOMPILING_OPTS_BOOST=""
	export CONF_CROSSCOMPILING_OPTS_VPX=""
	export CONF_CROSSCOMPILING_OPTS_X264=""
	export CONF_CROSSCOMPILING_OPTS_FFMPEG=""
	#export CC=`which gcc`
	#export CXX=`which g++`
	if [ "$USE_LLVM" = "1" ];
	then
		export CC=`which clang`
		export CXX=`which clang++`
		export AS=`which llvm-as`
		export AR=`which llvm-ar`
		#export LD=`which llvm-ld`
		export LD=`which lld`
		export RANLIB=`which llvm-ranlib`
		export OBJCOPY=`which llvm-objcopy`
		export OBJDUMP=`which llvm-objdump`
		export NM=`which llvm-nm`
	else
		if [ "$UNIX_SYSTEM_NAME" = "Linux" ];
		then
			if [ -z "${CC}" ];
			then
				export CC=`which gcc-7`
				if [ -z "${CC}" ];
				then
					export CC=`which gcc`
				fi
			fi
			if [ -z "${CXX}" ];			
			then
				export CXX=`which g++-7`
				if [ -z "${CXX}" ];
				then
					export CXX=`which g++`
				fi
			fi
		else
			export CC=`which gcc`
			export CXX=`which g++`
		fi
		export AS=`which as`
		export AR=`which ar`
		export LD=`which ld`
		export RANLIB=`which ranlib`
		export OBJCOPY=`which objcopy`
		export OBJDUMP=`which objdump`
		export NM=`which nm`
	fi
	export STRIP=`which strip`
	export UPNP_DISABLE_LARGE_FILE_SUPPORT=""
else
	export HELPER_ARM_TOOLCHAIN_NAME=arm-linux-gnueabihf

	if [ ! -d "$ARM_TOOLCHAIN_PATH" ];
	then
		export ARM_TOOLCHAIN_LINK="https://drive.google.com/file/d/11z-0nJpOBECycQxTpBxLC9cwXwYCFOjt/view?usp=sharing"
		export ARM_TOOLCHAIN_INTERNAL_LINK="http://store.skale.lan/files/gcc7.2-arm-toolchaine.tar.gz"
		export ARM_TOOLCHAIN_ARCH_NAME=$ARM_TOOLCHAIN_NAME-toolchaine.tar.gz

		mkdir -p $TOOLCHAINS_PATH
		if [ ! -d $TOOLCHAINS_PATH ];
		then
			echo " "
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			echo -e "${COLOR_ERROR}error: ${COLOR_VAR_VAL}${TOOLCHAINS_PATH}${COLOR_ERROR} folder not created!${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Create ${COLOR_VAR_VAL}${TOOLCHAINS_PATH}${COLOR_ERROR} folder and give permissions for writing here to current user.${COLOR_RESET}"
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			cd $WORKING_DIR_OLD
			env_restore_original
			exit -1
		fi

		mkdir -p $TOOLCHAINS_DOWNLOADED_PATH
		cd $TOOLCHAINS_DOWNLOADED_PATH
		wget $ARM_TOOLCHAIN_INTERNAL_LINK

		if [ ! -f $ARM_TOOLCHAIN_ARCH_NAME ];
		then
			echo " "
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Cannot download toolchain archive: ${COLOR_VAR_VAL}$ARM_TOOLCHAIN_ARCH_NAME${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Try download: ${COLOR_VAR_VAL}${ARM_TOOLCHAIN_LINK}${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Mirror: ${COLOR_VAR_VAL}${ARM_TOOLCHAIN_INTERNAL_LINK}${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Copy ${COLOR_VAR_VAL}${ARM_TOOLCHAIN_ARCH_NAME}${COLOR_ERROR} to ${COLOR_VAR_VAL}${TOOLCHAINS_DOWNLOADED_PATH}${COLOR_RESET}"
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			cd $WORKING_DIR_OLD
			env_restore_original
			exit -1
		fi

		mkdir -p $ARM_TOOLCHAIN_PATH
		cd $ARM_TOOLCHAIN_PATH
		tar -zxvf $TOOLCHAINS_DOWNLOADED_PATH/$ARM_TOOLCHAIN_ARCH_NAME

		if [ ! -d "$ARM_TOOLCHAIN_PATH/arm-linux-gnueabihf/bin" ];
		then
			echo " "
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			echo -e "${COLOR_ERROR}Cannot unpack toolchain archive: ${COLOR_VAR_VAL}$TOOLCHAINS_DOWNLOADED_PATH${COLOR_ERROR}/${COLOR_VAR_VAL}$TOOLCHAIN_ARCH_NAME${COLOR_RESET}"
			echo -e "${COLOR_SEPARATOR}=================================================${COLOR_RESET}"
			cd $WORKING_DIR_OLD
			env_restore_original
			exit -1
		fi

		echo -e "${COLOR_SEPARATOR}============== ${COLOR_PROJECT_NAME}TOOLCHAINE UNPACKED${COLOR_SEPARATOR} ==============${COLOR_RESET}"
	fi

	set -e -o pipefail

	export TOOLCHAIN=$ARM_TOOLCHAIN_NAME

	export ARM_BOOST_PATH=$ARM_TOOLCHAIN_PATH/boost
#	export PATH="$ARM_TOOLCHAIN_PATH/arm-linux-gnueabihf/bin:$ARM_TOOLCHAIN_PATH/bin:$PATH"
	export LD_LIBRARY_PATH="$ARM_TOOLCHAIN_PATH/arm-linux-gnueabihf/lib:$ARM_TOOLCHAIN_PATH/lib:$ARM_TOOLCHAIN_PATH/lib/gcc/arm-linux-gnueabihf/$ARM_GCC_VER/plugin:$LD_LIBRARY_PATH"

	export CC="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-gcc"
	export CXX="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-g++"
	export RANLIB="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-ranlib"
	export AR="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-ar"
	export LD="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-ld"
	export AS="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-as"
	export STRIP="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-strip"
	export NM="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-nm"
	export OBJCOPY="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-objcopy"
	export OBJDUMP="$ARM_TOOLCHAIN_PATH/bin/arm-linux-gnueabihf-objdump"

	export ADDITIONAL_INCLUDES="-I$ARM_TOOLCHAIN_PATH/arm-linux-gnueabihf/include -I$ARM_TOOLCHAIN_PATH/lib/gcc/arm-linux-gnueabihf/$ARM_GCC_VER/include -I$ARM_BOOST_PATH/include -I$INSTALL_ROOT/include"
	export ADDITIONAL_LIBRARIES="-L$ARM_TOOLCHAIN_PATH/arm-linux-gnueabihf/lib -L$ARM_TOOLCHAIN_PATH/lib -L$ARM_TOOLCHAIN_PATH/lib/gcc/arm-linux-gnueabihf/$ARM_GCC_VER/plugin -L$ARM_BOOST_PATH/lib -L$LIBRARIES_ROOT"

	export RPATHS="-Wl,-rpath,/opt/skale/lib -Wl,-rpath,/lib/arm-linux-gnueabihf"
	export CFLAGS="$ADDITIONAL_INCLUDES $ADDITIONAL_LIBRARIES $RPATHS -w $CFLAGS"
	export CXXFLAGS="$ADDITIONAL_INCLUDES $ADDITIONAL_LIBRARIES $RPATHS -w $CXXFLAGS"

	export CMAKE_CROSSCOMPILING_OPTS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON CMAKE_C_COMPILER=$CC CMAKE_CXX_COMPILER=$CXX"

	export CONF_CROSSCOMPILING_OPTS_GENERIC="--host=arm-linux"
	export CONF_CROSSCOMPILING_OPTS_VORBIS="--host=arm-linux --target=$ARM_TOOLCHAIN_PATH/bin/$HELPER_ARM_TOOLCHAIN_NAME"
	export CONF_CROSSCOMPILING_OPTS_CURL="--host=$ARM_TOOLCHAIN_PATH/bin/$HELPER_ARM_TOOLCHAIN_NAME --target=armv7-linux-gcc"
	export CONF_CROSSCOMPILING_OPTS_BOOST="toolset=gcc-arm target-os=linux"
	export CONF_CROSSCOMPILING_OPTS_VPX="--target=armv7-linux-gcc --cpu=cortex-a7"
	export CONF_CROSSCOMPILING_OPTS_X264="--host=arm-linux --disable-asm --disable-opencl"
	export CONF_CROSSCOMPILING_OPTS_FFMPEG="--enable-cross-compile --cross-prefix=$ARM_TOOLCHAIN_PATH/bin/$HELPER_ARM_TOOLCHAIN_NAME- --arch=armel --target-os=linux --disable-asm"
	export UPNP_DISABLE_LARGE_FILE_SUPPORT="--disable-largefile"
fi

if [ -z "${CC}" ];
then
	echo -e "${COLOR_ERROR}error: build requires gcc compiler or link which was not detected successfully${COLOR_RESET}"
	cd $WORKING_DIR_OLD
	env_restore_original
	exit -1
fi
if [ -z "${CXX}" ];
then
	echo -e "${COLOR_ERROR}error: build requires g++ compiler or link which was not detected successfully${COLOR_RESET}"
	cd $WORKING_DIR_OLD
	env_restore_original
	exit -1
fi
export CMAKE="$CMAKE -DUSE_LLVM=$USE_LLVM -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_LINKER=$LD -DCMAKE_AR=$AR -DCMAKE_OBJCOPY=$OBJCOPY -DCMAKE_OBJDUMP=$OBJDUMP -DCMAKE_RANLIB=$RANLIB -DCMAKE_NM=$NM"
#
echo -e "${COLOR_VAR_NAME}WORKING_DIR_OLD${COLOR_DOTS}........${COLOR_VAR_DESC}Started in directory${COLOR_DOTS}...................${COLOR_VAR_VAL}$WORKING_DIR_OLD${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WORKING_DIR_NEW${COLOR_DOTS}........${COLOR_VAR_DESC}Switched to directory${COLOR_DOTS}..................${COLOR_VAR_VAL}$WORKING_DIR_NEW${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}UNIX_SYSTEM_NAME${COLOR_DOTS}.......${COLOR_VAR_DESC}Building on host${COLOR_DOTS}.......................${COLOR_VAR_VAL}$UNIX_SYSTEM_NAME${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}NUMBER_OF_CPU_CORES${COLOR_DOTS}....${COLOR_VAR_DESC}Building on host having CPU cores${COLOR_DOTS}......${COLOR_VAR_VAL}$NUMBER_OF_CPU_CORES${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}ARCH${COLOR_DOTS}...................${COLOR_VAR_DESC}Building for architecture${COLOR_DOTS}..............${COLOR_VAR_VAL}$ARCH${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}DEBUG${COLOR_DOTS}.........................................................${COLOR_VAR_VAL}$DEBUG${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}TOP_CMAKE_BUILD_TYPE${COLOR_DOTS}...${COLOR_VAR_DESC}Building confiuration${COLOR_DOTS}..................${COLOR_VAR_VAL}$TOP_CMAKE_BUILD_TYPE${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CUSTOM_BUILD_ROOT${COLOR_DOTS}......${COLOR_VAR_DESC}Building in directory${COLOR_DOTS}..................${COLOR_VAR_VAL}$CUSTOM_BUILD_ROOT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}SOURCES_ROOT${COLOR_DOTS}...........${COLOR_VAR_DESC}Libraries source directory${COLOR_DOTS}.............${COLOR_VAR_VAL}$SOURCES_ROOT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}PREDOWNLOADED_ROOT${COLOR_DOTS}.....${COLOR_VAR_DESC}Pre-downloaded directory${COLOR_DOTS}...............${COLOR_VAR_VAL}$PREDOWNLOADED_ROOT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}INSTALL_ROOT${COLOR_DOTS}...........${COLOR_VAR_DESC}Install directory(prefix)${COLOR_DOTS}..............${COLOR_VAR_VAL}$INSTALL_ROOT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}PARALLEL_COUNT${COLOR_DOTS}................................................${COLOR_VAR_VAL}$PARALLEL_COUNT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}PARALLEL_MAKE_OPTIONS${COLOR_DOTS}.........................................${COLOR_VAR_VAL}$PARALLEL_MAKE_OPTIONS${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}DYLD_LIBRARY_PATH${COLOR_DOTS}.............................................${COLOR_VAR_VAL}$DYLD_LIBRARY_PATH${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}USE_LLVM${COLOR_DOTS}......................................................${COLOR_VAR_VAL}$USE_LLVM${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CC${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$CC${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CXX${COLOR_DOTS}...........................................................${COLOR_VAR_VAL}$CXX${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}MAKE${COLOR_DOTS}..........................................................${COLOR_VAR_VAL}$MAKE${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CMAKE${COLOR_DOTS}.........................................................${COLOR_VAR_VAL}$CMAKE${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CCMAKE${COLOR_DOTS}........................................................${COLOR_VAR_VAL}$CCMAKE${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}SCONS${COLOR_DOTS}.........................................................${COLOR_VAR_VAL}$SCONS${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WGET${COLOR_DOTS}..........................................................${COLOR_VAR_VAL}$WGET${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AUTOCONF${COLOR_DOTS}......................................................${COLOR_VAR_VAL}$AUTOCONF${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AUTOGEN${COLOR_DOTS}.......................................................${COLOR_VAR_VAL}$AUTOGEN${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AUTOMAKE${COLOR_DOTS}......................................................${COLOR_VAR_VAL}$AUTOMAKE${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}M4${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$M4${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}LIBTOOL${COLOR_DOTS}.......................................................${COLOR_VAR_VAL}$LIBTOOL${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}SHTOOL${COLOR_DOTS}........................................................${COLOR_VAR_VAL}$SHTOOL${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}PKG_CONFIG${COLOR_DOTS}....................................................${COLOR_VAR_VAL}$PKG_CONFIG${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}SED${COLOR_DOTS}...........................................................${COLOR_VAR_VAL}$SED${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AWK${COLOR_DOTS}...........................................................${COLOR_VAR_VAL}$AWK${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}YASM${COLOR_DOTS}..........................................................${COLOR_VAR_VAL}$YASM${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}NASM${COLOR_DOTS}..........................................................${COLOR_VAR_VAL}$NASM${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AS${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$AS${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CC${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$CC${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}CXX${COLOR_DOTS}...........................................................${COLOR_VAR_VAL}$CXX${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}AR${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$AR${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}LD${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$LD${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}STRIP${COLOR_DOTS}.........................................................${COLOR_VAR_VAL}$STRIP${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}RANLIB${COLOR_DOTS}........................................................${COLOR_VAR_VAL}$RANLIB${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}NM${COLOR_DOTS}............................................................${COLOR_VAR_VAL}$NM${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}OBJCOPY${COLOR_DOTS}.......................................................${COLOR_VAR_VAL}$OBJCOPY${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}OBJDUMP${COLOR_DOTS}.......................................................${COLOR_VAR_VAL}$OBJDUMP${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_ZLIB${COLOR_DOTS}..............${COLOR_VAR_DESC}Zlib${COLOR_DOTS}...................................${COLOR_VAR_VAL}$WITH_ZLIB${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_OPENSSL${COLOR_DOTS}...........${COLOR_VAR_DESC}OpenSSL${COLOR_DOTS}................................${COLOR_VAR_VAL}$WITH_OPENSSL${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_CURL${COLOR_DOTS}..............${COLOR_VAR_DESC}CURL${COLOR_DOTS}...................................${COLOR_VAR_VAL}$WITH_CURL${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_LZMA${COLOR_DOTS}..............${COLOR_VAR_DESC}LZMA${COLOR_DOTS}...................................${COLOR_VAR_VAL}$WITH_LZMA${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_SSH${COLOR_DOTS}...............${COLOR_VAR_DESC}SSH${COLOR_DOTS}....................................${COLOR_VAR_VAL}$WITH_SSH${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_SDL${COLOR_DOTS}...............${COLOR_VAR_DESC}SDL${COLOR_DOTS}....................................${COLOR_VAR_VAL}$WITH_SDL${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_SDL_TTF${COLOR_DOTS}...........${COLOR_VAR_DESC}SDL-TTF${COLOR_DOTS}................................${COLOR_VAR_VAL}$WITH_SDL_TTF${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_EV${COLOR_DOTS}................${COLOR_VAR_DESC}libEv${COLOR_DOTS}..................................${COLOR_VAR_VAL}$WITH_EV${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_EVENT${COLOR_DOTS}.............${COLOR_VAR_DESC}libEvent${COLOR_DOTS}...............................${COLOR_VAR_VAL}$WITH_EVENT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_UV${COLOR_DOTS}................${COLOR_VAR_DESC}libUV${COLOR_DOTS}..................................${COLOR_VAR_VAL}$WITH_UV${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_LWS${COLOR_DOTS}...............${COLOR_VAR_DESC}libWevSockets${COLOR_DOTS}..........................${COLOR_VAR_VAL}$WITH_LWS${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_SOURCEY${COLOR_DOTS}...........${COLOR_VAR_DESC}libSourcey${COLOR_DOTS}.............................${COLOR_VAR_VAL}$WITH_SOURCEY${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_BOOST${COLOR_DOTS}.............${COLOR_VAR_DESC}libBoostC++${COLOR_DOTS}............................${COLOR_VAR_VAL}$WITH_BOOST${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_PUPNP${COLOR_DOTS}.............${COLOR_VAR_DESC}libpupnp${COLOR_DOTS}...............................${COLOR_VAR_VAL}$WITH_PUPNP${COLOR_RESET}"
#echo -e "${COLOR_VAR_NAME}WITH_GTEST${COLOR_DOTS}.............${COLOR_VAR_DESC}GTEST${COLOR_DOTS}..................................${COLOR_VAR_VAL}$WITH_GTEST${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_ARGTABLE2${COLOR_DOTS}.........${COLOR_VAR_DESC}libArgTable${COLOR_DOTS}............................${COLOR_VAR_VAL}$WITH_ARGTABLE2${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_NETTLE${COLOR_DOTS}............${COLOR_VAR_DESC}LibNettle${COLOR_DOTS}..............................${COLOR_VAR_VAL}$WITH_NETTLE${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_TASN1${COLOR_DOTS}.............${COLOR_VAR_DESC}libTASN1${COLOR_DOTS}...............................${COLOR_VAR_VAL}$WITH_TASN1${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_GNU_TLS${COLOR_DOTS}...........${COLOR_VAR_DESC}libGnuTLS${COLOR_DOTS}..............................${COLOR_VAR_VAL}$WITH_GNU_TLS${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_GPGERROR${COLOR_DOTS}..........${COLOR_VAR_DESC}libGpgError${COLOR_DOTS}............................${COLOR_VAR_VAL}$WITH_GPGERROR${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_GCRYPT${COLOR_DOTS}............${COLOR_VAR_DESC}libGCrypt${COLOR_DOTS}..............................${COLOR_VAR_VAL}$WITH_GCRYPT${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_MICRO_HTTP_D${COLOR_DOTS}......${COLOR_VAR_DESC}libMiniHttpD${COLOR_DOTS}...........................${COLOR_VAR_VAL}$WITH_MICRO_HTTP_D${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_JSONCPP${COLOR_DOTS}...........${COLOR_VAR_DESC}LibJsonC++${COLOR_DOTS}.............................${COLOR_VAR_VAL}$WITH_JSONCPP${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_JSONRPCCPP${COLOR_DOTS}........${COLOR_VAR_DESC}LibJsonRpcC++${COLOR_DOTS}..........................${COLOR_VAR_VAL}$WITH_JSONRPCCPP${COLOR_RESET}"
echo -e "${COLOR_VAR_NAME}WITH_CRYPTOPP${COLOR_DOTS}..........${COLOR_VAR_DESC}LibCrypto++${COLOR_DOTS}............................${COLOR_VAR_VAL}$WITH_CRYPTOPP${COLOR_RESET}"

#
#
#

cd $SOURCES_ROOT

env_save() {
	export > $SOURCES_ROOT/saved_environment_pre_configured.txt
}

env_restore() {
	ENV_RESTORE_CMD="source ${SOURCES_ROOT}/saved_environment_pre_configured.txt"
	#env_clear_all
	$ENV_RESTORE_CMD
}

# we will save env now, next times we will only restore it)
env_save

#
#
#

# echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}BZIP2${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
# if [ ! -f "$INSTALL_ROOT/lib/libbz2.a" ];
# then
# 	# https://github.com/VFR-maniac/bzip2
# 	env_restore
# 	cd $SOURCES_ROOT
# 	if [ ! -d "bzip2" ];
# 	then
# 		#if [ ! -f "bzip2-from-git.tar.gz" ];
# 		#then
# 		#		echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
# 		#		git clone https://github.com/VFR-maniac/bzip2
# 		#		echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
# 		#		tar -czf bzip2-from-git.tar.gz ./bzip2
# 		#else
# 		#		echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 		#		tar -xzf bzip2-from-git.tar.gz
# 		#fi
# 		#
# 		# l_sergiy: moved into $PREDOWNLOADED_ROOT
# 		#
# 		echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 		tar -xzf $PREDOWNLOADED_ROOT/bzip2-from-git.tar.gz
# 		#
# 		#
# 		#
# 		if [ ! -f "Makefile.original.saved" ];
# 		then
# 			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 			cd bzip2
# 			cp Makefile Makefile.original.saved
# 			awk '{gsub("CC=gcc", "", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 			awk '{gsub("CXX=g++", "", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 			awk '{gsub("AR=ar", "", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 			awk '{gsub("RANLIB=ranlib", "", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
#
# 			if [ "$ARCH" = "arm" ];
# 			then
# 				awk '{gsub("all: libbz2.a bzip2 bzip2recover test", "all: libbz2.a bzip2 bzip2recover", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 				awk '{gsub("LDFLAGS=", "", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 				awk '{gsub("CFLAGS=", "CFLAGS:=$(CFLAGS) ", $0); print}' Makefile > Makefile.tmp; mv -f Makefile.tmp Makefile
# 			fi
#
# 			cd ..
# 		fi
# 	fi
# 	echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
# 	cd bzip2
# 	$MAKE $PARALLEL_MAKE_OPTIONS $DIRECT_MAKE_CROSSCOMPILING_OPTS
# 	$MAKE $PARALLEL_MAKE_OPTIONS $DIRECT_MAKE_CROSSCOMPILING_OPTS install PREFIX=$INSTALL_ROOT
# 	cd ..
# 	cd $SOURCES_ROOT
# else
# 	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# fi

# if [ "$WITH_LZMA" = "yes" ];
# then
# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}LZMA${COLOR_SEPARATOR} =========================================${COLOR_RESET}"
# 	if [ ! -f "$INSTALL_ROOT/lib/liblzma.a" ];
# 	then
# 		env_restore
# 		cd $SOURCES_ROOT
# 		export PKG_CONFIG_PATH_SAVED=$PKG_CONFIG_PATH
# 		export PKG_CONFIG_PATH=/$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
# 		if [ ! -d "lzma" ];
# 		then
# 			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 			tar -xzf $PREDOWNLOADED_ROOT/lzma-from-git.tar.gz
# 			#
# 			#
# 			#
# 			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 			cd lzma
# 			mkdir -p build
# 			cd build
# 			cmake $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
# 			cd ../..
# 		fi
# 		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
# 		cd lzma/build
# 		$MAKE $PARALLEL_MAKE_OPTIONS
# 		$MAKE $PARALLEL_MAKE_OPTIONS install
# 		cd ..
# 		export PKG_CONFIG_PATH=$PKG_CONFIG_PATH_SAVED
# 		export PKG_CONFIG_PATH_SAVED=
# 		cd $SOURCES_ROOT
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi
# fi

if [ "$WITH_ZLIB" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}ZLIB${COLOR_SEPARATOR} =========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libz.a" ];
	then
		## (required for libssh)
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "zlib" ];
		then
			if [ ! -f "zlib-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
				git clone https://github.com/madler/zlib.git
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf zlib-from-git.tar.gz ./zlib
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf zlib-from-git.tar.gz
			fi
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd zlib
			./configure --static --prefix=$INSTALL_ROOT
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd zlib
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

#echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}LIBXML2${COLOR_SEPARATOR} =======================${COLOR_RESET}"
#if [ ! -f "$INSTALL_ROOT/lib/libxml2.a" ];
#then
#	env_restore
#	cd $SOURCES_ROOT
#	if [ ! -d "libxml2" ];
#	then
#		echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
#		git clone git://git.gnome.org/libxml2
#		echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
#		cd libxml2
#		echo "--------------___> ${INSTALL_ROOT}"
#		./autogen.sh --prefix=$INSTALL_ROOT --host=arm-linux --disable-shared --without-python
#		cd ..
#	fi
#	echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
#    cd libxml2
#
#    CFLAGS=`xml2-config --cflags`
#    LIBS=`xml2-config --libs`
#
#	$MAKE $PARALLEL_MAKE_OPTIONS
#	$MAKE $PARALLEL_MAKE_OPTIONS install
#	mv $INSTALL_ROOT/include/libxml2 $INSTALL_ROOT/include
#
#	#cp -r $INSTALL_ROOT/liblibxml2/include $INSTALL_ROOT/include
#	#cp -r $INSTALL_ROOT/liblibxml2/include $INSTALL_ROOT/include
#	#rm -rf $INSTALL_ROOT/include/libxml2
#	#cd ..
#	#cd $SOURCES_ROOTls
#else
#	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
#fi

if [ "$WITH_OPENSSL" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}Open SSL${COLOR_SEPARATOR} =====================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libssl.a" ];
	then
		## openssl
		## https://www.openssl.org/
		## https://wiki.openssl.org/index.php/Compilation_and_Installation
		## (required for libssh)
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "openssl" ];
		then
			if [ ! -f "openssl-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
				git clone https://github.com/openssl/openssl.git
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf openssl-from-git.tar.gz ./openssl
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf openssl-from-git.tar.gz
			fi
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd openssl
			git fetch
			#git checkout OpenSSL_1_0_1-stable
			#git checkout OpenSSL_1_0_2-stable
			#git checkout OpenSSL_1_0_2g
			#git checkout OpenSSL_1_1_0-stable
			git checkout OpenSSL_1_1_1-stable
			if [ "$ARCH" = "x86_or_x64" ];
			then
				if [ "$UNIX_SYSTEM_NAME" = "Darwin" ];
				then
					export KERNEL_BITS=64
					./Configure darwin64-x86_64-cc -fPIC no-shared --prefix=$INSTALL_ROOT
				else
					./config -fPIC no-shared --prefix=$INSTALL_ROOT --openssldir=$INSTALL_ROOT
				fi
			else
				./Configure linux-armv4 --prefix=$INSTALL_ROOT $ADDITIONAL_INCLUDES $ADDITIONAL_LIBRARIES no-shared no-tests no-dso
				#./Configure linux-armv4 --prefix=/work/deps_inst/arm no-shared no-dso no-engine
				#./Configure linux-armv4 --prefix=$INSTALL_ROOT no-shared no-dso no-engine
				#./Configure linux-armv4 --prefix=$INSTALL_ROOT no-shared no-dso no-engine no-asm
				#./Configure linux-armv4 --prefix=$INSTALL_ROOT no-shared no-dso no-asm
			fi
			#$MAKE $PARALLEL_MAKE_OPTIONS depend
			#$MAKE depend
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd openssl
		$MAKE $PARALLEL_MAKE_OPTIONS depend
		$MAKE $PARALLEL_MAKE_OPTIONS
		##$MAKE $PARALLEL_MAKE_OPTIONS install
		$MAKE $PARALLEL_MAKE_OPTIONS install_sw
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

##########################sudo apt-get install libsasl2-dev
#echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}SASL${COLOR_SEPARATOR} =========================================${COLOR_RESET}"
#if [ ! -f "$INSTALL_ROOT/lib/libsasl2.a" ];
#then
#	env_restore
#	cd $SOURCES_ROOT
#	if [ ! -d "cyrus-sasl" ];
#	then
#		if [ ! -f "sasl-from-git.tar.gz" ];
#		then
#			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
#			git clone git@github.com:cyrusimap/cyrus-sasl.git
#			cd cyrus-sasl
#			#git fetch
#			#git checkout cyrus-sasl-2.1
#			#git pull
#			cd ..
#			echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
#			tar -czf sasl-from-git.tar.gz ./cyrus-sasl
#		else
#			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
#			tar -xzf sasl-from-git.tar.gz
#		fi
#	fi
#	cd cyrus-sasl
#	./autogen.sh --prefix=$INSTALL_ROOT --enable-static
#	$MAKE $PARALLEL_MAKE_OPTIONS
#	$MAKE $PARALLEL_MAKE_OPTIONS install
#	cd $SOURCES_ROOT
#else
#	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
#	cd $SOURCES_ROOT
#fi

if [ "$WITH_SSH" = "yes" ];
then
	#
	# built with errors everywhere
	#
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}LibSSH${COLOR_SEPARATOR} =======================================${COLOR_RESET}"
	if [ ! -d "$INSTALL_ROOT/include/libssh" ];
	then
		# https://www.libssh.org
		# https://stackoverflow.com/questions/16248775/cmake-not-able-to-find-openssl
		# git clone https://git.libssh.org/projects/libssh.git libssh
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libssh" ];
		then
		if [ ! -f "libssh-from-git.tar.gz" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
			git clone git://git.libssh.org/projects/libssh.git libssh
			echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
			tar -czf libssh-from-git.tar.gz ./libssh
		else
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf libssh-from-git.tar.gz
		fi
		echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
		cd libssh
		mkdir -p build
		cd build
		$CMAKE -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DLIBSSH_STATIC=1 -DOPENSSL_ROOT_DIR=$SOURCES_ROOT/openssl/ -DOPENSSL_LIBRARIES=$SOURCES_ROOT/openssl/ -DZLIB_ROOT=$SOURCES_ROOT/zlib/ -DCMAKE_BUILD_TYPE=Release
		..
		cd ..
		cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libssh/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ..
		cd ..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_CURL" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}CURL${COLOR_SEPARATOR} =========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libcurl.a" ];
	then
		# https://github.com/curl/curl
		env_restore
		cd $SOURCES_ROOT
		export PKG_CONFIG_PATH_SAVED=$PKG_CONFIG_PATH
		export PKG_CONFIG_PATH=/$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
		if [ ! -d "curl" ];
		then
			if [ ! -f "curl-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
				git clone https://github.com/curl/curl.git
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf curl-from-git.tar.gz ./curl
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf curl-from-git.tar.gz
			fi
			#
			# l_sergiy: moved into $PREDOWNLOADED_ROOT
			#
	# # 		echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
	# # 		tar -xzf $PREDOWNLOADED_ROOT/curl-from-git.tar.gz
			#
			#
			#
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd curl
			mkdir -p build
			cd build
			#$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCMAKE_USE_LIBSSH2=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCURL_STATICLIB=ON -DOPENSSL_CRYPTO_LIBRARY=$INSTALL_ROOT/lib/libcrypto.a -DOPENSSL_INCLUDE_DIR=$INSTALL_ROOT/include -DOPENSSL_SSL_LIBRARY=$INSTALL_ROOT/lib/libssl.a CMAKE_C_COMPILER_WORKS=ON CMAKE_CXX_COMPILER_WORKS=ON ..
			cmake $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DOPENSSL_ROOT_DIR=$SOURCES_ROOT/openssl -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCMAKE_USE_LIBSSH2=OFF -DBUILD_SHARED_LIBS=OFF -DCURL_DISABLE_LDAP=ON -DCURL_STATICLIB=ON -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
			echo " " >> lib/curl_config.h
			echo "#define HAVE_POSIX_STRERROR_R 1" >> lib/curl_config.h
			echo " " >> lib/curl_config.h
			### Set HAVE_POSIX_STRERROR_R to 1 in build/lib/curl_config.h
			cd ../..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd curl/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		if [ "$DEBUG" = "1" ];
		then
			mv "$INSTALL_ROOT/lib/libcurl-d.a" "$INSTALL_ROOT/lib/libcurl.a" &> /dev/null
		fi
		cd ..
		export PKG_CONFIG_PATH=$PKG_CONFIG_PATH_SAVED
		export PKG_CONFIG_PATH_SAVED=
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}ICONV${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
if [ ! -f "$INSTALL_ROOT/lib/libiconv.a" ];
then
	if [ "$UNIX_SYSTEM_NAME" = "Darwin" ];
	then
		echo -e "${COLOR_SUCCESS}skipping iconv on $UNIX_SYSTEM_NAME )))${COLOR_RESET}"
	else
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libiconv-1.15" ];
		then
			if [ ! -f "libiconv-1.15.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				$WGET https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz
			fi
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf libiconv-1.15.tar.gz
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libiconv-1.15
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libiconv-1.15
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ..
		cd $SOURCES_ROOT
	fi
else
	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
fi

if [ "$WITH_SDL" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}SDL2${COLOR_SEPARATOR} =========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libSDL2.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "SDL2-2.0.7" ];
		then
			if [ ! -f "SDL2-2.0.7.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				$WGET https://www.libsdl.org/release/SDL2-2.0.7.tar.gz
			fi
			tar -xzf SDL2-2.0.7.tar.gz
			cd SDL2-2.0.7
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			./configure --enable-static --disable-shared --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd SDL2-2.0.7
		$MAKE $PARALLEL_MAKE_OPTIONS
		# ??? this parallel install does not work on OSX ???
		#$MAKE $PARALLEL_MAKE_OPTIONS install
		$MAKE install
		cd ..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_SDL_TTF" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}SDL2_ttf${COLOR_SEPARATOR} + ${COLOR_PROJECT_NAME}freetype${COLOR_SEPARATOR} ==========================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libSDL2_ttf.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		export PKG_CONFIG_PATH_SAVED=$PKG_CONFIG_PATH
		export PKG_CONFIG_PATH=/$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
		#
		#export LDFLAGS_SAVED_OLD=$LDFLAGS
		#export LDFLAGS="$INSTALL_ROOT/lib -liconv"
		#
		if [ ! -d "SDL2_ttf-2.0.14" ]; then
			if [ ! -f "SDL2_ttf-2.0.14.tar.gz" ]; then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				$WGET https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.14.tar.gz
			fi
			tar -xzf SDL2_ttf-2.0.14.tar.gz
			cd SDL2_ttf-2.0.14
			#pushd external/freetype-2.4.12
			#echo -e "${COLOR_INFO}configuring freetype${COLOR_DOTS}...${COLOR_RESET}"
			#./autogen.sh
			#./configure --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
			#popd
			#echo -e "${COLOR_INFO}configuring SDL2_ttf${COLOR_DOTS}...${COLOR_RESET}"
			#./autogen.sh
			#./configure --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT --with-sdl-prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
			cd ..
		fi
		cd SDL2_ttf-2.0.14
		echo -e "${COLOR_INFO}building freetype${COLOR_DOTS}...${COLOR_RESET}"
		pushd external/freetype-2.4.12
		./autogen.sh
		./configure --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
		# ??? this parallel build does not work on OSX ???
		#$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE
		# ??? this parallel install does not work on OSX ???
		#$MAKE $PARALLEL_MAKE_OPTIONS install
		$MAKE install
		popd
		echo -e "${COLOR_INFO}building SDL2_ttf${COLOR_DOTS}...${COLOR_RESET}"
		./autogen.sh
		./configure --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT --with-sdl-prefix=$INSTALL_ROOT --with-freetype-prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ..
		#
		#export LDFLAGS=$LDFLAGS_SAVED_OLD
		#export LDFLAGS_SAVED_OLD=
		#
		export PKG_CONFIG_PATH=$PKG_CONFIG_PATH_SAVED
		export PKG_CONFIG_PATH_SAVED=
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_EV" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libEv${COLOR_SEPARATOR} ================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libev.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libev" ];
		then
			if [ ! -f "libev-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				git clone git@github.com:LuaDist/libev.git
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf libev-from-git.tar.gz ./libev
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf libev-from-git.tar.gz
			fi
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libev
			mkdir -p build
			cd build
			#$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE -DARCH=$ARCH ..
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
			cd ../..
		fi
		cd libev/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ../..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_EVENT" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libEvent${COLOR_SEPARATOR} =====================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libevent.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libevent" ];
		then
			if [ ! -f "libevent-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				git clone git@github.com:libevent/libevent.git
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf libevent-from-git.tar.gz ./libevent
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf libevent-from-git.tar.gz
			fi
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libevent
			mkdir -p build
			cd build
			OS_SPECIFIC_LIB_EVENT_FLAGS=""
			if [ ${ARCH} = "arm" ]
			then
				OS_SPECIFIC_LIB_EVENT_FLAGS="-DEVENT__DISABLE_SAMPLES=ON -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_BENCHMARK=ON -DEVENT__DISABLE_REGRESS=ON"
			fi
			#$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE -DARCH=$ARCH ..
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS $OS_SPECIFIC_LIB_EVENT_FLAGS -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
			cd ../..
		fi
		cd libevent/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ../..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_UV" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libUV${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libuv.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libuv" ];
		then
			if [ ! -f "libuv-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
                                git clone https://github.com/libuv/libuv.git
				cd libuv
				git checkout v1.x
				git pull
				cd ..
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf libuv-from-git.tar.gz ./libuv
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf libuv-from-git.tar.gz
			fi
			#
			# l_sergiy: moved into $PREDOWNLOADED_ROOT
			#
# # # 			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# # # 			#tar -xzf $PREDOWNLOADED_ROOT/libuv-from-git.tar.gz
# # # 			tar -xzf $PREDOWNLOADED_ROOT/libuv-modified.tar.gz
			#
			#
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libuv
			./autogen.sh
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
			#--with-sysroot==$INSTALL_ROOT
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libuv
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_LWS" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libWebSockets${COLOR_SEPARATOR} ================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libwebsockets.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libwebsockets" ];
		then
			if [ ! -f "libwebsockets-from-git.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
                                git clone https://github.com/warmcat/libwebsockets.git
				cd libwebsockets
				git checkout v3.1-stable
				git pull
				cd ..
				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
				tar -czf libwebsockets-from-git.tar.gz ./libwebsockets
			else
				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
				tar -xzf libwebsockets-from-git.tar.gz
			fi
			#
			# l_sergiy: ... if moved into $PREDOWNLOADED_ROOT ...
			#
			#echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			#tar -xzf $PREDOWNLOADED_ROOT/libwebsockets-modified.tar.gz
			#tar -xzf $PREDOWNLOADED_ROOT/libwebsockets-from-git.tar.gz
			#
			#
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libwebsockets
			mkdir -p build
			cd build
			LWS_WITH_LIBEV=OFF
			LWS_WITH_LIBEVENT=OFF
			LWS_WITH_LIBUV=OFF
			#if [ "$WITH_EV" = "yes" ];
			#then
			#	if [ ! -f "$INSTALL_ROOT/lib/libev.a" ];
			#	then
			#		#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEV=OFF"
			#		echo " "
			#	else
			#		#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEV=ON"
			#		LWS_WITH_LIBEV=ON
			#	fi
			#else
			##	#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEV=OFF"
			#	echo " "
			#fi
			if [ "$WITH_EVENT" = "yes" ];
			then
				if [ ! -f "$INSTALL_ROOT/lib/libevent.a" ];
				then
					#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEVENT=OFF"
					echo " "
				else
					#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEVENT=ON"
					LWS_LIBEVENT_OPTIONS="-DLWS_WITH_LIBEVENT=ON -DLWS_LIBEVENT_INCLUDE_DIRS=$INSTALL_ROOT/include -DLWS_LIBEVENT_LIBRARIES=$INSTALL_ROOT/lib/libevent.a"
				fi
			else
				#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBEVENT=OFF"
				echo " "
				LWS_LIBEVENT_OPTIONS="-DLWS_WITH_LIBEVENT=OFF"
			fi
			if [ "$WITH_UV" = "yes" ];
			then
				if [ ! -f "$INSTALL_ROOT/lib/libuv.a" ];
				then
					#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBUV=OFF"
					echo " "
				else
					#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBUV=ON"
					LWS_LIBUV_OPTIONS="-DLWS_WITH_LIBUV=ON -DLWS_LIBUV_INCLUDE_DIRS=$INSTALL_ROOT/include -DLWS_LIBUV_LIBRARIES=$INSTALL_ROOT/lib/libuv.a"
				fi
			else
				#CMAKE_ARGS_FOR_LIB_WEB_SOCKETS="$CMAKE_ARGS_FOR_LIB_WEB_SOCKETS -DLWS_WITH_LIBUV=OFF"
				echo " "
				LWS_LIBUV_OPTIONS="-DLWS_WITH_LIBUV=OFF"
			fi
			#
			#
			#
			# TO DO: on "Darwin" add this to CMakeLists.txt
			#set( CMAKE_C_FLAGS   "${CMAKE_CXX_FLAGS} -Wno-error -Wno-implicit-function-declaration" )
			#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error -Wno-implicit-function-declaration" )
			#fi
			#
			#
			#
			#$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE $CMAKE_ARGS_FOR_LIB_WEB_SOCKETS ..
			export SAVED_CFLAGS=$CFLAGS
			export CFLAGS="$CFLAGS -Wno-deprecated-declarations"
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE \
				-DLWS_WITH_STATIC=ON -DLWS_WITH_SHARED=OFF -DLWS_STATIC_PIC=ON \
				-DLWS_IPV6=ON -DLWS_UNIX_SOCK=ON -DLWS_WITH_HTTP2=OFF -DLWS_WITHOUT_TESTAPPS=ON \
				-DLWS_WITH_ACCESS_LOG=ON -DLWS_WITH_SERVER_STATUS=ON \
				-DLWS_WITH_LIBEV=$LWS_WITH_LIBEV $LWS_LIBEVENT_OPTIONS $LWS_LIBUV_OPTIONS \
				-DLWS_HAVE_LIBCAP=OFF -DLWS_MAX_SMP=1024 \
				-DLWS_WITH_THREADPOOL=1 \
				-DLWS_WITH_HTTP2=1 \
				-DLWS_WITH_SSL=ON \
				-DLWS_OPENSSL_INCLUDE_DIRS="$INSTALL_ROOT/include/openssl" \
         		-DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE=/usr/local/ssl \
				-DZLIB_INCLUDE_DIR="$INSTALL_ROOT/include" \
				..
			export CFLAGS=$SAVED_CFLAGS
			#
			#
			#-DLWS_WITH_HTTP2=ON
			#
			#-DLWS_SSL_SERVER_WITH_ECDH_CERT=OFF ???
			#-DLWS_WITH_CGI=OFF
			#-DLWS_HAVE_OPENSSL_ECDH_H=1 ???
			#-DLWS_HAVE_SSL_CTX_set1_param=1 ???
			#-DLWS_HAVE_RSA_SET0_KEY=   ???
			#-DLWS_WITH_HTTP_PROXY=OFF ???
			#-DLWS_WITH_LEJP=OFF ???
			#-DLWS_WITH_LEJP_CONF=OFF ???
			#-DLWS_WITH_SMTP=OFF ???
			#-DLWS_WITH_GENERIC_SESSIONS=OFF ??? ??? ???
			#-DLWS_WITH_RANGES=ON ???
			#-DLWS_WITH_ZIP_FOPS = ON ???
			#-DLWS_AVOID_SIGPIPE_IGN = OFF ??? ??? ???
			#-DLWS_WITH_STATS=OFF ??? ??? ???
			#-DLWS_WITH_SOCKS5=OFF ???
			#-DLWS_HAVE_LIBCAP=  ???
			#-DLWS_WITH_PEER_LIMITS=OFF ??? ??? ???
			#-DLIBHUBBUB_LIBRARIES=    ???
			#-DPLUGINS=     ???
			#-DLWS_ROLE_DBUS=1
			#-DLWS_WITH_LWSAC=1
			#-DLWS_WITH_LWSAC=1
			#-DLWS_WITH_HTTP_STREAM_COMPRESSION=1 for gzip
			#-DLWS_WITH_HTTP_BROTLI=1 for preferred br brotli compression
			#-DLWS_WITH_HTTP_PROXY=1 and -DLWS_UNIX_SOCK=1
			#-DLWS_AVOID_SIGPIPE_IGN ???
			cd ../..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libwebsockets/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd ../..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_SOURCEY" = "yes" ];
then
    echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libSourcey${COLOR_SEPARATOR} ================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libscy_base.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libsourcey" ];
		then
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf $PREDOWNLOADED_ROOT/libsourcey-1.1.4.tar.gz

			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libsourcey
			mkdir -p build
			cd build
			$CMAKE .. -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE \
				-DCMAKE_LIBRARY_PATH=$LIBS_ROOT \
				-DBUILD_SHARED_LIBS=OFF \
				-DBUILD_DEPENDENCIES=OFF -DBUILD_APPLICATIONS=OFF -DBUILD_MODULES=OFF \
				-DBUILD_SAMPLES=OFF -DBUILD_TESTS=OFF -DBUILD_MODULE_base=ON \
				-DBUILD_MODULE_http=ON -DBUILD_MODULE_net=ON \
				-DBUILD_MODULE_util=ON -DBUILD_MODULE_uv=OFF -DBUILD_MODULE_crypto=ON \
				-DWITH_LIBUV=OFF -DWITH_ZLIB=OFF \
				-DOPENSSL_ROOT_DIR=${INSTALL_ROOT} -DOPENSSL_LIBRARIES=${LIBS_ROOT}
			cd ..
			cd ..
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libsourcey/build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd $SOURCES_ROOT
		else
			echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
		fi
fi

# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}ASIO${COLOR_SEPARATOR} ======================================${COLOR_RESET}"
# 	if [ ! -d "$INSTALL_ROOT/include/asio" ];
# 	then
# 		# (required for libssh)
#		env_restore
# 		cd $SOURCES_ROOT
# 		if [ ! -d "asio" ]; then
# 			if [ ! -f "asio-from-git.tar.gz" ]; then
# 				echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
# 				git clone https://github.com/chriskohlhoff/asio.git
# 				echo -e "${COLOR_INFO}archiving it${COLOR_DOTS}...${COLOR_RESET}"
# 				tar -cvzf asio-from-git.tar.gz ./asio
# 			else
# 				echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 				tar -xvzf asio-from-git.tar.gz
# 			fi
# 			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 			cd asio/asio
# 			./autogen.sh
# 			./configure --prefix=$INSTALL_ROOT --without-boost --with-openssl=$SOURCES_ROOT/openssl $CONF_DEBUG_OPTIONS
# 			cd ../..
# 		fi
# 		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
# 		cd asio/asio
# 		$MAKE $PARALLEL_MAKE_OPTIONS
# 		$MAKE $PARALLEL_MAKE_OPTIONS install
# 		cd ../..
# 		cd $SOURCES_ROOT
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi

if [ "$WITH_BOOST" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}BOOST${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libboost_system.a" ];
	then
		#####https://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.gz
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "boost_1_68_0" ];
		then
			if [ ! -f "boost_1_68_0.tar.gz" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				$WGET https://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.gz
			fi
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf boost_1_68_0.tar.gz
		fi
		cd boost_1_68_0
		echo -e "${COLOR_INFO}configuring and building it${COLOR_DOTS}...${COLOR_RESET}"

		./bootstrap.sh --prefix=$INSTALL_ROOT --with-libraries=system,thread,filesystem,regex,atomic

	if [ ${ARCH} = "arm" ]
	then
		sed -i -e 's#using gcc ;#using gcc : arm : /usr/local/toolchains/gcc7.2-arm/bin/arm-linux-gnueabihf-g++ ;#g' project-config.jam
		./b2 $CONF_CROSSCOMPILING_OPTS_BOOST cxxflags=-fPIC cflags=-fPIC $PARALLEL_MAKE_OPTIONS --prefix=$INSTALL_ROOT --layout=system variant=debug link=static threading=multi install
		else
	#	    sed -i -e 's#using gcc ;#using gcc : arm : /usr/bin/g++-7 ;#g' project-config.jam
		./b2 cxxflags=-fPIC cflags=-fPIC $PARALLEL_MAKE_OPTIONS --prefix=$INSTALL_ROOT --layout=system variant=debug link=static threading=multi install
	fi
		cd ..
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_PUPNP" = "yes" ];
then
	# http://pupnp.sourceforge.net/
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}pupnp${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libupnp.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libupnp-1.8.3" ];
		then
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xvjf $PREDOWNLOADED_ROOT/libupnp-1.8.3.tar.bz2
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libupnp-1.8.3
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $UPNP_DISABLE_LARGE_FILE_SUPPORT $CONF_DEBUG_OPTIONS
			cd $SOURCES_ROOT
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd libupnp-1.8.3
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi


#echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}READLINE${COLOR_SEPARATOR} ======================${COLOR_RESET}"
#if [ ! -f "$INSTALL_ROOT/lib/libreadline.a" ];
#then
#	env_restore
#    cd $SOURCES_ROOT
#    if [ ! -d "readline-7.0" ];
#    then
#        if [ ! -f "readline-7.0.tar.gz" ];
#        then
#            echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
#            $WGET https://ftp.gnu.org/gnu/readline/readline-7.0.tar.gz
#        fi
#        echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
#        tar -xzf readline-7.0.tar.gz
#        echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
#        cd readline-7.0
#        mkdir -p build
#        cd build
#        ../configure $CONF_CROSSCOMPILING_OPTS_GENERIC --host=arm-linux --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
#        cd $SOURCES_ROOT
#    fi
#    cd readline-7.0
#    cd build
#    make $PARALLEL_MAKE_OPTIONS
#    make $PARALLEL_MAKE_OPTIONS install
#    cd ..
#    cd $SOURCES_ROOT
#else
#	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
#fi

# echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}LIBXML2${COLOR_SEPARATOR} ======================${COLOR_RESET}"
# if [ ! -f "$INSTALL_ROOT/lib/libxml2.so" ];
# then
# 	env_restore
# 	cd $SOURCES_ROOT
# 	if [ ! -d "libxml2-2.9.7" ];
# 	then
# 		if [ ! -f "libxml2-2.9.7.tar.gz" ];
# 		then
# 			echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
# 			$WGET http://xmlsoft.org/sources/libxml2-2.9.7.tar.gz
# 		fi
# 		echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 		tar -xzf libxml2-2.9.7.tar.gz
# 		echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 		cd libxml2-2.9.7
# 		mkdir -p build
# 		cd build
# 		../configure $CONF_CROSSCOMPILING_OPTS_GENERIC --host=arm-linux --without-html --without-python --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
# 		cd $SOURCES_ROOT
# 	fi
# 	cd libxml2-2.9.7
# 	cd build
# 	make $PARALLEL_MAKE_OPTIONS
# 	make $PARALLEL_MAKE_OPTIONS install
# 	cd ..
# 	cd $SOURCES_ROOT
# else
# 	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# fi

# echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}LIBARCHIVE${COLOR_SEPARATOR} ======================${COLOR_RESET}"
# if [ ! -f "$INSTALL_ROOT/lib/libarchive.so" ];
# then
# 	env_restore
# 	cd $SOURCES_ROOT
# 	if [ ! -d "libarchive-3.3.2" ];
# 	then
# 		if [ ! -f "libarchive-3.3.2.tar.gz" ];
# 		then
# 			echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
# 			$WGET https://www.libarchive.org/downloads/libarchive-3.3.2.tar.gz
# 		fi
# 		echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
# 		tar -xzf libarchive-3.3.2.tar.gz
# 		echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 		cd libarchive-3.3.2
# 		mkdir -p build
# 		cd build
# 		../configure $CONF_CROSSCOMPILING_OPTS_GENERIC --host=arm-linux --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
# 		cd $SOURCES_ROOT
# 	fi
# 	cd libarchive-3.3.2/build
# 	make $PARALLEL_MAKE_OPTIONS
# 	make $PARALLEL_MAKE_OPTIONS install
# 	cd ..
# 	cd $SOURCES_ROOT
# else
# 	echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# fi


# if [ "$WITH_GTEST" = "1" ];
# then
# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}GTEST${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
#
# 	if [ ! -f "$INSTALL_ROOT/lib/libgtest.a" ];
# 	then
# 		env_restore
# 		cd $SOURCES_ROOT
#
# 		if [ ! -d gtest ];
# 		then
# 			git clone https://github.com/google/googletest.git gtest
# 		fi
#
# 		cd gtest
#
# 		mkdir -p build
# 		cd build
#
# 		$CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
# 		$MAKE $PARALLEL_MAKE_OPTIONS
# 		$MAKE $PARALLEL_MAKE_OPTIONS install
#
# 		if [ $DEBUG = "1" ];
# 		then
# 			mv "$LIBRARIES_ROOT/libgtestd.a" "$LIBRARIES_ROOT/libgtest.a"
# 			mv "$LIBRARIES_ROOT/libgtest_maind.a" "$LIBRARIES_ROOT/libgtest_main.a"
# 			mv "$LIBRARIES_ROOT/libgmockd.a" "$LIBRARIES_ROOT/libgmock.a"
# 			mv "$LIBRARIES_ROOT/libgmock_maind.a" "$LIBRARIES_ROOT/libgmock_main.a"
# 		fi
#
# 		cd $SOURCES_ROOT
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi
# fi
#
# if [[ "$WITH_V8" = "yes" ]]
# then
# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}V8${COLOR_SEPARATOR} ========================================${COLOR_RESET}"
# 	if [ ! -d v8 ];
# 	then
# 		git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
# 		export SAVED_PATH=$PATH
# 		export PATH=$PATH:`realpath depot_tools`
#
# 		gclient
# 		fetch v8
# 		cd v8
#
# 		gn gen out/x64/Release --args='is_debug=false is_clang=false target_cpu="x64" treat_warnings_as_errors=false is_component_build=false use_custom_libcxx=false v8_use_snapshot=false v8_static_library=true'
# 		ninja -C out/x64/Release
#
# 		export PATH=$SAVED_PATH
# 		rm -rf depot_tools
# 		rm .gclient
# 		rm .gclient_entries
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi
# fi


#https://github.com/jonathanmarvens/argtable2
#git@github.com:jonathanmarvens/argtable2.git
if [ "$WITH_ARGTABLE2" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libArgTable${COLOR_SEPARATOR} ==================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libargtable2${DEBUG_D}.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "argtable2" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
                        git clone https://github.com/jonathanmarvens/argtable2.git
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd argtable2
			mkdir -p build
			cd build
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE ..
			cd ..
		else
			cd argtable2
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_NETTLE" = "yes" ];
then
    echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libNettle${COLOR_SEPARATOR} ====================================${COLOR_RESET}"
    if [ ! -f "$INSTALL_ROOT/lib/libnettle.a" ];
    then
        env_restore
        cd $SOURCES_ROOT
        if [ ! -d "nettle-3.4.1" ];
        then
            if [ ! -f "nettle-3.4.1.tar.gz" ];
            then
                echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
                $WGET https://ftp.gnu.org/gnu/nettle/nettle-3.4.1.tar.gz
            fi
            tar -xvzf nettle-3.4.1.tar.gz
            echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
            cd nettle-3.4.1
            ./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
        else
            cd nettle-3.4.1
        fi
        echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
        $MAKE $PARALLEL_MAKE_OPTIONS
        $MAKE $PARALLEL_MAKE_OPTIONS install
        cd $SOURCES_ROOT
    else
        echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
    fi
fi

if [ "$WITH_TASN1" = "yes" ];
then
    echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libtasn1${COLOR_SEPARATOR} =====================================${COLOR_RESET}"
    if [ ! -f "$INSTALL_ROOT/lib/libtasn1.a" ];
    then
        env_restore
        cd $SOURCES_ROOT
        #export PKG_CONFIG_PATH_SAVED=$PKG_CONFIG_PATH
        #export PKG_CONFIG_PATH=/$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
        if [ ! -d "libtasn1" ];
        then
            echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
            git clone git@github.com:gnutls/libtasn1.git
            echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
            cd libtasn1
            test -f ./configure || autoreconf --install
            aclocal
            autoconf
            autoheader
            automake --add-missing
            ./configure --disable-doc --disable-gtk-doc=1 --disable-gtk-doc-html --disable-gtk-doc-pdf \
                $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
        else
            cd libtasn1
        fi
        echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
        $MAKE $PARALLEL_MAKE_OPTIONS
        $MAKE $PARALLEL_MAKE_OPTIONS install
        #export PKG_CONFIG_PATH=$PKG_CONFIG_PATH_SAVED
        #export PKG_CONFIG_PATH_SAVED=
        cd $SOURCES_ROOT
    else
        echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
    fi
fi

# if [ "$WITH_GNU_TLS" = "yes" ];
# then
# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libGnuTLS${COLOR_SEPARATOR} ====================================${COLOR_RESET}"
# 	if [ ! -f "$INSTALL_ROOT/lib/libgnutls.a" ];
# 	then
# 		env_restore
# 		cd $SOURCES_ROOT
# 		export PKG_CONFIG_PATH_SAVED=$PKG_CONFIG_PATH
# 		export PKG_CONFIG_PATH=/$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
# 		if [ ! -d "gnutls-3.6.5" ];
# 		then
# 			if [ ! -f "gnutls-3.6.5.tar.xz" ];
# 			then
# 				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
# 				$WGET https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-3.6.5.tar.xz
# 			fi
# 			tar -xf gnutls-3.6.5.tar.xz
# 			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 			cd gnutls-3.6.5
# 			#export PKG_CONFIG_PATH=$INSTALL_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
#             ./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --prefix=$INSTALL_ROOT --with-nettle-mini=$SOURCES_ROOT/nettle-3.4.1 --with-libnettle-prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
#         else
# 			cd gnutls-3.6.5
# 		fi
# 		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
# 		$MAKE $PARALLEL_MAKE_OPTIONS
# 		$MAKE $PARALLEL_MAKE_OPTIONS install
# 		export PKG_CONFIG_PATH=$PKG_CONFIG_PATH_SAVED
# 		export PKG_CONFIG_PATH_SAVED=
# 		cd $SOURCES_ROOT
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi
# fi

#https://github.com/gnutls/gnutls
#git@github.com:gnutls/gnutls.git
if [ "$WITH_GNU_TLS" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libGnuTLS${COLOR_SEPARATOR} ====================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libgnutls.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "gnutls" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
            git clone git@github.com:gnutls/gnutls.git
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd gnutls
			./bootstrap
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
		else
			cd gnutls
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		$MAKE
		$MAKE install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

#https://github.com/gpg/libgpg-error
#git@github.com:gpg/libgpg-error.git
if [ "$WITH_GPGERROR" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}liGpgError${COLOR_SEPARATOR} ===================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libgpg-error.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libgpg-error" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
            git clone git@github.com:gpg/libgpg-error.git
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libgpg-error
			./autogen.sh --git-build
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
		else
			cd libgpg-error
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		$MAKE
		$MAKE install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

#https://github.com/gpg/libgcrypt
#git@github.com:gpg/libgcrypt.git
if [ "$WITH_GCRYPT" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}liGrypt${COLOR_SEPARATOR} ======================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libgcrypt.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libgcrypt" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
            git clone git@github.com:gpg/libgcrypt.git
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libgcrypt
			./autogen.sh
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $CONF_DEBUG_OPTIONS
		else
			cd libgcrypt
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		$MAKE
		$MAKE install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

#https://github.com/scottjg/libmicrohttpd
#git@github.com:scottjg/libmicrohttpd.git
if [ "$WITH_MICRO_HTTP_D" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libMiniHttpD${COLOR_SEPARATOR} =================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libmicrohttpd.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libmicrohttpd" ];
		then
			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
            git clone https://github.com/scottjg/libmicrohttpd.git
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libmicrohttpd
			MHD_HTTPS_OPT=""
			if [ "$WITH_GCRYPT" = "yes" ];
			then
				MHD_HTTPS_OPT="--enable-https"
			fi
			./bootstrap
			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT $MHD_HTTPS_OPT $CONF_DEBUG_OPTIONS
		else
			cd libmicrohttpd
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		$MAKE
		$MAKE install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

# #https://github.com/Sajoch/libmicrohttpd_openssl
# #git@github.com:Sajoch/libmicrohttpd_openssl.git
# if [ "$WITH_MICRO_HTTP_D" = "yes" ];
# then
# 	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libMiniHttpD${COLOR_SEPARATOR} =================================${COLOR_RESET}"
# 	if [ ! -f "$INSTALL_ROOT/lib/libmicrohttpd.a" ];
# 	then
# 		env_restore
# 		cd $SOURCES_ROOT
# 		if [ ! -d "libmicrohttpd_openssl" ];
# 		then
# 			echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
#             git clone git@github.com:Sajoch/libmicrohttpd_openssl.git
# 			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
# 			cd libmicrohttpd_openssl
# 			./bootstrap
# 			./configure $CONF_CROSSCOMPILING_OPTS_GENERIC --enable-static --disable-shared --with-pic --prefix=$INSTALL_ROOT --enable-https $CONF_DEBUG_OPTIONS
# 		else
# 			cd libmicrohttpd_openssl
# 		fi
# 		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
# 		$MAKE
# 		$MAKE install
# 		cd $SOURCES_ROOT
# 	else
# 		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
# 	fi
# fi

#https://github.com/open-source-parsers/jsoncpp
#git@github.com:open-source-parsers/jsoncpp.git
if [ "$WITH_JSONCPP" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libJsonC++${COLOR_SEPARATOR} ===================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libjsoncpp.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "jsonpp" ];
		then
			#
			#echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
			#git clone git@github.com:open-source-parsers/jsoncpp.git
			#
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf $PREDOWNLOADED_ROOT/jsoncpp.tar.gz
			#
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd jsoncpp
			mkdir -p build
			cd build
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE \
				-DBUILD_SHARED_LIBS=NO \
				-DBUILD_STATIC_LIBS=YES \
				..
			cd ..
		else
			cd jsoncpp
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

#https://github.com/cinemast/libjson-rpc-cpp
#git@github.com:cinemast/libjson-rpc-cpp.git
if [ "$WITH_JSONRPCCPP" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libJsonRpcC++${COLOR_SEPARATOR} ================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libjsonrpccpp-server.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libjson-rpc-cpp" ];
		then
			#
			#echo -e "${COLOR_INFO}getting it from git${COLOR_DOTS}...${COLOR_RESET}"
			#git clone git@github.com:cinemast/libjson-rpc-cpp.git
			#
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			tar -xzf $PREDOWNLOADED_ROOT/libjson-rpc-cpp.tar.gz
			#
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
			cd libjson-rpc-cpp
			mkdir -p build
			cd build
			$CMAKE $CMAKE_CROSSCOMPILING_OPTS -DCMAKE_INSTALL_PREFIX=$INSTALL_ROOT -DCMAKE_BUILD_TYPE=$TOP_CMAKE_BUILD_TYPE \
				-DBUILD_SHARED_LIBS=NO \
				-DBUILD_STATIC_LIBS=YES \
				-DUNIX_DOMAIN_SOCKET_SERVER=YES \
				-DUNIX_DOMAIN_SOCKET_CLIENT=YES \
				-DFILE_DESCRIPTOR_SERVER=YES \
				-DFILE_DESCRIPTOR_CLIENT=YES \
				-DTCP_SOCKET_SERVER=YES \
				-DTCP_SOCKET_CLIENT=YES \
				-DREDIS_SERVER=NO \
				-DREDIS_CLIENT=NO \
				-DHTTP_SERVER=YES \
				-DHTTP_CLIENT=YES \
				-DCOMPILE_TESTS=NO \
				-DCOMPILE_STUBGEN=YES \
				-DCOMPILE_EXAMPLES=NO \
				-DWITH_COVERAGE=NO \
				-DARGTABLE_INCLUDE_DIR=$SOURCES_ROOT/argtable2/src \
				-DARGTABLE_LIBRARY=$INSTALL_ROOT/lib/libargtable2${DEBUG_D}.a \
				-DJSONCPP_INCLUDE_DIR=$INSTALL_ROOT/include \
				..
			cd ..
		else
			cd libjson-rpc-cpp
		fi
		echo -e "${COLOR_INFO}building it${COLOR_DOTS}...${COLOR_RESET}"
		cd build
		$MAKE $PARALLEL_MAKE_OPTIONS
		$MAKE $PARALLEL_MAKE_OPTIONS install
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

if [ "$WITH_CRYPTOPP" = "yes" ];
then
	echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}libCrypto++${COLOR_SEPARATOR} ==================================${COLOR_RESET}"
	if [ ! -f "$INSTALL_ROOT/lib/libcryptopp.a" ];
	then
		env_restore
		cd $SOURCES_ROOT
		if [ ! -d "libcryptopp" ];
		then
			mkdir libcryptopp
			if [ ! -f "cryptopp700.zip" ];
			then
				echo -e "${COLOR_INFO}downloading it${COLOR_DOTS}...${COLOR_RESET}"
				$WGET https://www.cryptopp.com/cryptopp700.zip
			fi
			echo -e "${COLOR_INFO}unpacking it${COLOR_DOTS}...${COLOR_RESET}"
			unzip cryptopp700.zip -d $SOURCES_ROOT/libcryptopp
			echo -e "${COLOR_INFO}configuring it${COLOR_DOTS}...${COLOR_RESET}"
		fi
		cd $SOURCES_ROOT/libcryptopp
		$MAKE $PARALLEL_MAKE_OPTIONS static
		$MAKE $PARALLEL_MAKE_OPTIONS install PREFIX=$INSTALL_ROOT
		cd $SOURCES_ROOT
	else
		echo -e "${COLOR_SUCCESS}SKIPPED${COLOR_RESET}"
	fi
fi

echo -e "${COLOR_SEPARATOR}==================== ${COLOR_PROJECT_NAME}FINISH${COLOR_SEPARATOR} =======================================${COLOR_RESET}"
#env_restore
#cd $CUSTOM_BUILD_ROOT
cd $WORKING_DIR_OLD
env_restore_original
exit 0
