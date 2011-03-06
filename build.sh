#!/bin/sh -e
# this sh script just makes it easier for me to make the zip package lol

# go to sh script directory
cd "$(dirname "$0")"

# clean up
echo "cleaning..."
make clean > /dev/null
rm -f openps3ftp.zip

# create openps3ftp.zip and add in the README, changelog, and COPYING files
echo "creating zip..."
touch README changelog COPYING
zip -q openps3ftp.zip README changelog COPYING

# compile and make pkgs then add it to the zip

# nopass
echo "creating nopass and adding to zip..."
sed -i 's/DISABLE_PASS\t[01]/DISABLE_PASS\t1/' ./include/common.h
make pkg > /dev/null
mv openps3ftp.pkg openps3ftp-nopass.pkg
mv openps3ftp.geohot.pkg openps3ftp-nopass.geohot.pkg 
zip -q openps3ftp.zip openps3ftp-nopass.pkg openps3ftp-nopass.geohot.pkg

# normal
echo "creating normal and adding to zip..."
sed -i 's/DISABLE_PASS\t[01]/DISABLE_PASS\t0/' ./include/common.h
make pkg > /dev/null
zip -q openps3ftp.zip openps3ftp.pkg openps3ftp.geohot.pkg

echo "cleaning..."
make clean > /dev/null
echo "done"
# end
