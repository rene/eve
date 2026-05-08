#! /bin/bash

openssl ecparam -name prime256v1 -genkey -noout -outform DER -out $1/private.der
openssl ec -inform DER -in $1/private.der -outform DER -pubout -out $1/public.der

DIR=`dirname $(readlink -f $0)`
UUID=`$DIR/ta_uuid $1/public.der ecc`

mv ${1}/"private.der" ${1}/${UUID}"_private.der"
mv ${1}/"public.der" ${1}/${UUID}"_public.der"

echo "TA_NAME = "$UUID > names.mk
echo "TC_NAME = "${UUID}_client >> names.mk

CUUID=`$DIR/ta_uuid $UUID`

sed -i "/#define TA_UUID/c\\${CUUID}" include/user_ta_header_defines.h
