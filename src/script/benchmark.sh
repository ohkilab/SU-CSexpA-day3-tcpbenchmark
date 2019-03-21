#!/bin/bash

#SERVERHOST="localhost"
SERVERHOST="192.168.0.13"
#SERVERNAME="tcpserverc1"
#SERVERNAME="tcpserver"
#SERVERNAME="tcpservermt"
SERVERNAME="tcpservermp"
#SERVERNAME="tcpserverle"
#SERVERNAME="tcpserveruv"
SERVERDIR="local-copy/${SERVERNAME}/src"
SERVERPORT="10000"
SERVER=${SERVERDIR}/${SERVERNAME}
SERVERSCRIPTDIR="local-copy/tcpbenchmark/src/script"
PEAKVM=${SERVERSCRIPTDIR}/peakvm.sh
COUNTSOCKET=${SERVERSCRIPTDIR}/count_socket.sh
CLEANCOUNTSOCKET=${SERVERSCRIPTDIR}/clean_count_socket.sh
SETRESOURCE=${SERVERSCRIPTDIR}/set_resource.sh

LOGDIR=log/
if [ ! -d ${LOGDIR} ] ; then
	mkdir -p ${LOGDIR}
fi
LOGBASENAME=${SERVERNAME}
BENCHMARK=../tcpbenchmark

# BENCHMARK PARAMETERS
THLIST=("200" "600" "1000")
DELAYLIST=("0" "10" "100" "1000")

function clean_prev_process() {
	ssh ${SERVERHOST} killall ${SERVERNAME}
	ssh ${SERVERHOST} bash ${CLEANCOUNTSOCKET}
}

function benchmark() {
	TH=$1
	DELAY=$2

	# clean previous benchmark processs, just in case.
	clean_prev_process

	BENCHLOGFILEPATH=${LOGDIR}/${LOGBASENAME}.${TH}.${DELAY}

	# start server
	SERVERPARAM="${SERVERPORT} ${DELAY}"
	ssh ${SERVERHOST} ${SERVER} ${SERVERPARAM} &> /tmp/server.log &

	# run still benchmark
	COUNTSOCKETLOGFILEPATH=${BENCHLOGFILEPATH}.SOCKET
	ssh ${SERVERHOST} bash ${COUNTSOCKET} ${SERVERNAME} &> ${COUNTSOCKETLOGFILEPATH} &

	${BENCHMARK} ${SERVERHOST} ${SERVERPORT} 1 ${TH} 100 &> ${BENCHLOGFILEPATH}

	# run after benchmark
	VMPEAKLOGFILEPATH=${BENCHLOGFILEPATH}.VMPEAK
	ssh ${SERVERHOST} bash ${PEAKVM} ${SERVERNAME} &> ${VMPEAKLOGFILEPATH}

	# finish server
	ssh ${SERVERHOST} killall ${SERVERNAME}
	clean_prev_process
}

# Unset Kernel Security Function to avoid following blocks
# kernel: TCP: Possible SYN flooding on port ${SERVERPORT}.
echo "Unset Kernel SynCookies in SERVER."
ssh ${SERVERHOST} sudo sysctl net.ipv4.tcp_syncookies=0

# Increase Resources on Local/Server
bash set_resource.sh
ssh ${SERVERHOST} bash ${SETRESOURCE}

for TH in ${THLIST[@]} ; do
	for DELAY in ${DELAYLIST[@]}; do

		echo "START: ${TH}.${DELAY}"

		# benchmark
		benchmark ${TH} ${DELAY}

		# wait
		sleep 1

		echo "COMPLETE: ${TH}.${DELAY}"
	done
done

clean_prev_process
