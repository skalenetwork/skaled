#!/bin/bash
SRC_DIR=`pwd`
#
cd $SRC_DIR
git checkout develop
git submodule update; git fetch --all --recurse-submodules; git pull --all --recurse-submodules; git branch; git status
cd $SRC_DIR/mapreduce_consensus
git checkout master
git submodule update; git fetch --all --recurse-submodules; git pull --all --recurse-submodules; git branch; git status
#
cd $SRC_DIR/test/jsontests
git checkout SKALE-470-tests
git submodule update; git fetch --all --recurse-submodules; git pull --all --recurse-submodules; git branch; git status
#
cd $SRC_DIR
