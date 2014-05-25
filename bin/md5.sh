#!/bin/bash
# generate MD5 checksums for files in the passed directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
unamestr=`uname`
md5sum='md5sum'

function abspath() {
    $(cd $(dirname "$1") && pwd -P)/$(basename "$1")
}

function checksums() {
    local files=`find ${1} -type f | egrep "(Makefile|\.(cpp|h|c|sh|ld|elf|a)$)" | grep -v grep | grep -v "${1}/tests/*"`
    $md5sum $files
}

if [[ "$unamestr" == 'Linux' ]]; then
    md5sum='md5sum'
elif [[ "$unamestr" == 'FreeBSD' || "$unamestr" == 'Darwin' ]]; then
    md5sum='md5'
fi

if [ ${#} -gt 0 ]; then
    path=`realpath ${1}`
    checksums ${path}
fi

