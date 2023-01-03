#!/usr/bin/env bash

set -e

: "${VERSION?Need to set VERSION}"
: "${BRANCH?Need to set BRANCH}"

NAME=schain
REPO_NAME=skalenetwork/$NAME
IMAGE_NAME=$REPO_NAME:$VERSION

LABEL="develop"
if [ $BRANCH = "stable" ]
then
    LABEL="stable"
elif [ $BRANCH = "beta" ]
then
    LABEL="beta"
fi
LATEST_IMAGE_NAME=$REPO_NAME:$LABEL-latest

if [[ $VERSION == *"historic" ]]
then
    LATEST_IMAGE_NAME=$LATEST_IMAGE_NAME-historic
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"


# Build image

echo "Building $IMAGE_NAME..."
docker build -t $IMAGE_NAME $DIR/skale_build || exit $?
docker tag $IMAGE_NAME $LATEST_IMAGE_NAME

echo "========================================================================================="
echo "Built $IMAGE_NAME"

# Publish image

: "${DOCKER_USERNAME?Need to set DOCKER_USERNAME}"
: "${DOCKER_PASSWORD?Need to set DOCKER_PASSWORD}"

echo "$DOCKER_PASSWORD" | docker login --username $DOCKER_USERNAME --password-stdin

docker push $IMAGE_NAME || exit $?

if [ $BRANCH = $LABEL ]
then
    docker push $LATEST_IMAGE_NAME || exit $?
fi
