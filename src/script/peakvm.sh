
#!/bin/bash

if [ "${1}" = "" ] ; then
	echo "Usage: peakvm.sh PATTERN"
	exit 0
fi
PATTERN=${1}
pgrep ${PATTERN} | xargs -IPID cat /proc/PID/status | grep VmPeak
