/*
 * timecounter.h
 *
 *  Created on: 2022/05/29
 *      Author: yasuh
 */
 
#ifndef TIMECOUNTER_H_
#define TIMECOUNTER_H_

#include <time.h>
 
typedef struct __timeCounter {
	struct timespec startTs;
	struct timespec endTs;
} TIMECOUNTER;

void countStart(TIMECOUNTER* tc);
void countEnd(TIMECOUNTER* tc);
double formatSec(struct timespec* ts);
double diffRealSec(TIMECOUNTER* tc);
void printTime(TIMECOUNTER* tc);
void printUsedTime(TIMECOUNTER* tc);

#endif

