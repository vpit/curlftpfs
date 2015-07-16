#!/bin/sh
mkdir -p -v m4
aclocal --install -Im4
autoreconf --verbose --install --symlink --force
