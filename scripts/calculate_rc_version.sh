#!/bin/bash

VERSION=$1

if [ -z $VERSION ]; then
      echo "The base version is not set."
      exit 1
fi

git fetch --tags

for (( RC_NUMBER=0; ; RC_NUMBER++ ))
do
    RC_VERSION="$VERSION-rc.$RC_NUMBER"
    if ! [ $(git tag -l ?$RC_VERSION) ]; then
        echo $RC_VERSION
        break
    fi
done
