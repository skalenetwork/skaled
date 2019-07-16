#!/bin/bash

TAG=$1

echo "Push tag $TAG"

git fetch --tags
git tag --list

git push origin $TAG
