#!/bin/bash

VERSION=`git describe`
sed -i "s/DUMMY_VERSION_DUMMY/${VERSION}/g" src/WIFIOnOff.ino
cat src/WIFIOnOff.ino
