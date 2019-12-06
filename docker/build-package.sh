#!/usr/bin/env bash
set -euo pipefail

mkdir build
cd build
cmake ..
cpack

export GPG_TTY=$(tty)
gpg --batch --import <(echo "$SIGN_KEY")
debsigs --sign=maint -k $SIGN_KEY_ID -v *.deb

dpkg-deb -I *.deb

tar cvfz /packages.tar.gz *.deb