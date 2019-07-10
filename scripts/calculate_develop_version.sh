#!/bin/bash

VERSION=$1

if [ -z $VERSION ]; then
      echo "The base version is not set."
      exit 1
fi

git fetch --tags

for (( DEVELOP_NUMBER=0; ; DEVELOP_NUMBER++ ))
do
    DEVELOP_VERSION="$VERSION-develop.$DEVELOP_NUMBER"
    if ! [ $(git tag -l ?$DEVELOP_VERSION) ]; then
        echo $DEVELOP_VERSION
        break
    fi
done
