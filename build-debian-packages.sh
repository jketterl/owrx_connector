#!/usr/bin/env bash
set -euo pipefail

SIGN_KEY_ID=EC56CED77C05107E4C416EF8173873AE062F3A10
SIGN_KEY=$(gpg --armor --export-secret-key $SIGN_KEY_ID)

for dist in buster; do
    docker build --pull --build-arg DIST=$dist -t owrx-connector-deb-builder:$dist .
    CONTAINER_NAME=owrx-connector-deb-builder
    docker run -it --name $CONTAINER_NAME -e SIGN_KEY_ID="$SIGN_KEY_ID" -e SIGN_KEY="$SIGN_KEY" owrx-connector-deb-builder:$dist
    docker cp $CONTAINER_NAME:/packages.tar.gz .
    docker rm $CONTAINER_NAME
    mkdir -p packages/$dist
    tar xvfz packages.tar.gz -C packages/$dist
    rm packages.tar.gz
done