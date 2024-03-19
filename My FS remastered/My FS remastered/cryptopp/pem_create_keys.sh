#!/usr/bin/env bash

# Script to create the test keys used pem_test.cxx

##################################
# prerequisites

if [[ -z "$CXX" ]]; then
    CXX=g++
fi

if [[ -z $(command -v "$CXX") ]]; then
    echo "Please install a compiler like g++"
    exit 1
fi

if [[ -z $(command -v openssl) ]]; then
    echo "Please install openssl package"
    exit 1
fi

if [[ -z $(command -v perl) ]]; then
    echo "Please install perl package"
    exit 1
fi

##################################
# test program

echo "Compiling test program"

rm -rf pem_test.exe &>/dev/null

CXXFLAGS="-DDEBUG -g3 -O0 -Wall"

# Build crypto++ library if out of date.
if ! CXX="$CXX" CXXFLAGS="$CXXFLAGS" make -j 4; then
    echo "Failed to build libcryptopp.a"
    exit 1
fi

# Build the test program
if ! "$CXX" "$CXXFLAGS" pem_test.cxx ./libcryptopp.a -o pem_test.exe; then
    echo "Failed to build pem_test.exe"
    exit 1
fi

##################################
# test keys

echo "Generating OpenSSL keys"

# RSA private key, public key, and encrypted private key
openssl genrsa -out rsa-priv.pem 1024
openssl rsa -in rsa-priv.pem -out rsa-pub.pem -pubout
openssl rsa -in rsa-priv.pem -out rsa-enc-priv.pem -aes128 -passout pass:abcdefghijklmnopqrstuvwxyz

# DSA private key, public key, and encrypted private key
openssl dsaparam -out dsa-params.pem 1024
openssl gendsa -out dsa-priv.pem dsa-params.pem
openssl dsa -in dsa-priv.pem -out dsa-pub.pem -pubout
openssl dsa -in dsa-priv.pem -out dsa-enc-priv.pem -aes128 -passout pass:abcdefghijklmnopqrstuvwxyz

# EC private key, public key, and encrypted private key
openssl ecparam -out ec-params.pem -name secp256k1 -genkey
openssl ec -in ec-params.pem -out ec-priv.pem
openssl ec -in ec-priv.pem -out ec-pub.pem -pubout
openssl ec -in ec-priv.pem -out ec-enc-priv.pem -aes128 -passout pass:abcdefghijklmnopqrstuvwxyz

openssl dhparam -out dh-params.pem 512

##################################
# malformed

# Only the '-----BEGIN PUBLIC KEY-----'
echo "-----BEGIN PUBLIC KEY-----" > rsa-short.pem

# Removes last CR or LF (or CRLF)
perl -pe 'chomp if eof' rsa-pub.pem > rsa-trunc-1.pem

# This gets the last CR or LF and one of the dashes (should throw)
perl -pe 'chop if eof' rsa-trunc-1.pem > rsa-trunc-2.pem

# Two keys in one file; missing CRLF between them
cat rsa-trunc-1.pem > rsa-concat.pem
cat rsa-pub.pem >> rsa-concat.pem

# Uses only CR (remove LF)
sed 's/\n//g' rsa-pub.pem > rsa-eol-cr.pem

# Uses only LF (remove CR)
sed 's/\r//g' rsa-pub.pem > rsa-eol-lf.pem

# No EOL (remove CR and LF)
sed 's/\r//g; s/\n//g' rsa-pub.pem > rsa-eol-none.pem

echo "-----BEGIN FOO-----" > foobar.pem
head -c 180 /dev/urandom | base64 -w 64 >> foobar.pem
echo "-----END BAR-----" >> foobar.pem

##################################
# cacert.pem

if [ ! -e "cacert.pem" ]; then
    wget http://curl.haxx.se/ca/cacert.pem -O cacert.pem
fi

