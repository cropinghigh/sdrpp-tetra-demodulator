#!/bin/sh

URL=https://www.etsi.org/deliver/etsi_en/300300_300399/30039502/01.03.01_60/en_30039502v010301p0.zip
MD5_EXP=a8115fe68ef8f8cc466f4192572a1e3e
LOCAL_FILE=etsi_tetra_codec.zip

PATCHDIR=`pwd`
CODECDIR=`pwd`/../codec

echo Deleting $CODECDIR ...
[ -e $CODECDIR ] && rm -rf $CODECDIR

echo Creating $CODECDIR ...
mkdir $CODECDIR

if [ ! -f $LOCAL_FILE ]; then
	echo Downloading $URL ...
# 	wget -O $LOCAL_FILE $URL
	curl -kLSs $URL -o $LOCAL_FILE
else
	echo Skipping download, file $LOCAL_FILE exists
fi
MD5=`md5sum $LOCAL_FILE | awk '{ print $1 }'`

echo Checking MD5SUM ...
if [ $MD5 != $MD5_EXP ]; then
	echo "MD5sum of ETSI reference codec file doesn't match"
	exit 1
fi

echo Unpacking ZIP ...
cd $CODECDIR
unzip -L $PATCHDIR/etsi_tetra_codec.zip
echo Contents of $CODECDIR:
ls -lah

echo Fixing filenames...
cd "$CODECDIR/c-code"

for f in *; do
    new="$(echo "$f" | tr 'I' 'i')"
    if [ "$f" != "$new" ]; then
        mv -i -- "$f" "$new"
    fi
done

cd $CODECDIR

echo Applying Patches ...
ls -lah $PATCHDIR
cat $PATCHDIR/series
for p in `cat "$PATCHDIR/series"`; do
	echo "=> Applying patch '$PATCHDIR/$p'..."
	patch -p1 -d "$CODECDIR" < "$PATCHDIR/$p"
done

echo Done!
