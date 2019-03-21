#!/bin/bash

if [ "${1}" = "" ] ; then
	echo "Usage: count_socker.sh PATTERN"
	exit 0
fi

PATTERN=${1}
START=false
END=0
while :
do
	SNum=`ps -ef | grep  ${PATTERN} | grep -v count_socket | grep -v grep | awk '{print $2}' | xargs -IPID lsof -p PID | grep TCP | wc -l`
	echo "S:"${SNum}
	PNum=`ps -ef | grep ${PATTERN} | grep -v count_socket | grep -v grep | wc -l`
	echo "P:"${PNum}
	if [ ${SNum} = "" ] ; then
		break
	fi

	if [ ${START} = "false" ] && [ ${SNum} -gt "1" ] ; then
		START=true
	fi

	if [ ${START} = "true" ] && [ ${SNum} -lt "2" ] ; then
		let END=${END}+1
	else
		END=0
	fi

	if [ ${END} -gt 10 ] ;then
		break
	fi
done
