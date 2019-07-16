#!/bin/bash

TAG=$1

echo "Push tag $TAG"

echo "fetch"
git fetch --tags

echo "list tags"
git tag --list

echo "push"
git push origin $TAG
