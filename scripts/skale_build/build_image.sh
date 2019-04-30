#!/bin/bash

VERSION=$1
IMAGE_NAME=skalelabshub/schain:$VERSION

echo "Build image $IMAGE_NAME"

docker build -t $IMAGE_NAME .
