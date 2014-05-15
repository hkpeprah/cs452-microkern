#!/bin/bash
FILENAME=`basename ${1}`
echo "Uploaded: ${FILENAME}"
ls /u/cs452/tftp/ARM/hkpeprah/
cp ${1} /u/cs452/tftp/ARM/hkpeprah/${FILENAME}
