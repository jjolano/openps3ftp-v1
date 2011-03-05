#!/bin/bash
# this sh script just makes it easier for me to make the zip package lol

# go to sh script directory
cd $(dirname $0)

# clean up
make clean
rm openps3ftp.zip

# create openps3ftp.zip and add in the README, changelog, and COPYING files
zip openps3ftp.zip README changelog COPYING

# compile and make pkgs then add it to the zip

# nopass
sed 's/DISABLE_PASS\t0/DISABLE_PASS\t1/' <./include/common.h >./include/common.h
make pkg && mv openps3ftp.pkg openps3ftp-nopass.pkg && mv openps3ftp.geohot.pkg openps3ftp-nopass.geohot.pkg && zip openps3ftp.zip openps3ftp-nopass.pkg openps3ftp-nopass.geohot.pkg

# normal
sed 's/DISABLE_PASS\t1/DISABLE_PASS\t0/' <./include/common.h >./include/common.h
make pkg && zip openps3ftp.zip openps3ftp.pkg openps3ftp.geohot.pkg

echo "done"
# end
