#!/bin/sh
rm -rf Final

cd Server
make clean
cd ../Client
make clean
