/*
 ============================================================================
 Name        : timecounter.c
 Author      : Yasuhiro Noguchi
 Version     : 0.0.1
 Copyright   : Copyright (C) HEPT Consortium
 Description : record start/end time and calculate the difference
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
#include "libclient.h"
#include "timecounter.h"

void countStart(TIMECOUNTER* tc) {
	clock_gettime(CLOCK_MONOTONIC, &(tc->startTs));
}

void countEnd(TIMECOUNTER* tc) {
	clock_gettime(CLOCK_MONOTONIC, &(tc->endTs));
}

/**
  * amount required time 
  */
double diffRealSec(TIMECOUNTER* tc) {
	time_t diffsec = difftime(tc->endTs.tv_sec, tc->startTs.tv_sec);
	long diffnanosec = tc->endTs.tv_nsec - tc->startTs.tv_nsec;
	return diffsec + diffnanosec*1e-9;
}

double formatSec(struct timespec* ts) {
	double r = ts->tv_sec + ts->tv_nsec*1e-9;
	return r;
}

void printTime(TIMECOUNTER* tc) {
	printf("%10.10f(sec), %10.10f(sec), %10.10f(sec)\n", formatSec(&(tc->startTs)), formatSec(&(tc->endTs)), diffRealSec(tc));
}

void printUsedTime(TIMECOUNTER* tc) {
	printf("UsedTime: %10.10f(sec)\n", diffRealSec(tc));
}

