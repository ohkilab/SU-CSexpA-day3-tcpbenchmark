#!/bin/bash
LOGDIR=$1
if [ "${LOGDIR}" = "" ] ; then
	echo "Usage: summarize.sh LOGDIR"
	exit 0
fi

for i in `find ${LOGDIR} | sort -n | grep -v SOCKET | grep -v VMPEAK | grep -v .sh | grep -v ${LOGDIR}$` ; do
	SERVER=`basename $i | awk -F. '{print $1}'`
	TH=`basename $i | awk -F. '{print $2}'`
	DELAY=`basename $i | awk -F. '{print $3}'`
	BRESULT=`grep ^${TH} $i | grep -v connection`
	VMPEAK=`cat $i.VMPEAK | sort -n | tail -n1`
	VMPEAK=`echo ${VMPEAK} | sed 's/ kB//g' | sed 's/VmPeak: //g'`
	SOCKETPEAK=`cat $i.SOCKET | grep ^S | sed 's/S://g' | sort -n | tail -n1`
	PSPEAK=`cat $i.SOCKET | grep ^P | sed 's/P://g' | sort -n | tail -n1`
	echo ${SERVER},${DELAY},${BRESULT},${VMPEAK},${SOCKETPEAK},${PSPEAK}
done

