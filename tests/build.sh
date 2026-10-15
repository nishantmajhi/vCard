#!/bin/sh
# Builds and runs the vCard parser tests. Needs only a C++17 compiler --
# nothing here links against Qt.
set -e
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
"$CXX" -std=c++17 -Wall -Wextra -O1 -o tests/vcard_test.exe tests/vcard_test.cpp src/vcard.cpp
exec ./tests/vcard_test.exe
