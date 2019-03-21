#!/bin/bash

for i in $(ps -ef | grep count_socket.sh | grep -v grep | awk '{print $2}') ; do
	kill $i
done
