#!/bin/sh -e
# go to sh script directory
cd "$(dirname "$0")"

ZIPFILE="openps3ftp.zip"
PKGFILE="$(basename $(pwd))"
SEDFILE="./include/common.h"

# clean up directory
echo "cleaning up..."
make clean > /dev/null
rm -f "$ZIPFILE"

# make pkg then add it to the zip
## create 'nopass' version
echo "compiling 'nopass' version..."
sed -i 's/DISABLE_PASS\t[01]/DISABLE_PASS\t1/' "$SEDFILE"
make pkg > /dev/null
mv "$PKGFILE.pkg" "$PKGFILE-nopass.pkg"
mv "$PKGFILE.geohot.pkg" "$PKGFILE-nopass.geohot.pkg"

## create 'normal' version
echo "compiling 'normal' version..."
sed -i 's/DISABLE_PASS\t[01]/DISABLE_PASS\t0/' "$SEDFILE"
make pkg > /dev/null

# create the zip file
echo "creating $ZIPFILE..."
touch README COPYING changelog *.pkg
zip "$ZIPFILE" README COPYING changelog *.pkg

# print hashes
echo "md5sums:"
md5sum *.pkg "$ZIPFILE"

# done, clean up
echo "cleaning up..."
make clean > /dev/null
echo "done"
# end
