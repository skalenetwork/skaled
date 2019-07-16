#!/bin/bash

TAG=$1

echo "Push tag $TAG"
git push origin $TAG
