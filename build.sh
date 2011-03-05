#!/bin/bash
# this sh script just makes it easier for me to make the zip package lol

# go to sh script directory
cd $(dirname $0)

# clean up
echo "cleaning..."
make clean >> /dev/null
rm openps3ftp.zip -f

# create openps3ftp.zip and add in the README, changelog, and COPYING files
echo "creating zip..."
touch README changelog COPYING
zip openps3ftp.zip README changelog COPYING -q

# compile and make pkgs then add it to the zip

cp ./include/common.h ./temp.h

# nopass
echo "creating nopass and adding to zip..."
sed 's/DISABLE_PASS\t0/DISABLE_PASS\t1/' <./temp.h >./include/common.h
make pkg >> /dev/null && mv openps3ftp.pkg openps3ftp-nopass.pkg && mv openps3ftp.geohot.pkg openps3ftp-nopass.geohot.pkg && zip openps3ftp.zip openps3ftp-nopass.pkg openps3ftp-nopass.geohot.pkg -q

# normal
echo "creating normal and adding to zip..."
sed 's/DISABLE_PASS\t1/DISABLE_PASS\t0/' <./temp.h >./include/common.h
make pkg >> /dev/null && zip openps3ftp.zip openps3ftp.pkg openps3ftp.geohot.pkg -q

echo "cleaning..."
make clean >> /dev/null
rm ./temp.h -f
echo "done"
# end
