#!/bin/bash
FILENAME=`basename ${1}`
echo "Uploaded: ${FILENAME}"
ls /u/cs452/tftp/ARM/${2}/
cp ${1} /u/cs452/tftp/ARM/${2}/${FILENAME}
chmod a+r /u/cs452/tftp/ARM/${2}/${FILENAME}
