#!/bin/sh
rm -rf Final

cd Server
make
cd ../Client
make
cd ..

mkdir Final
cp ./Server/bin/Release/Server ./Final/Server
cp ./Client/bin/Release/Client ./Final/Client
