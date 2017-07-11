/*
 ============================================================================
 Name        : TCPBenchmark.c
 Author      : Yasuhiro Noguchi
 Version     : 0.0.1
 Copyright   : Copyright (C) HEPT Consortium
 Description : TCP Connection Benchmark
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/time.h>
#include "libclient.h"

// ���������X���b�h�𓯎��X�^�[�g�����邽�߂̃t���O
volatile bool isRunnable = false;

typedef struct __timeCounter {
	struct timeval startTime;
	struct timeval endTime;
} TIMECOUNTER;

double getTimeOfDayBySec() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

void countStart(TIMECOUNTER* tc) {
	gettimeofday(&(tc->startTime), NULL);
}

void countEnd(TIMECOUNTER* tc) {
	gettimeofday(&(tc->endTime), NULL);
}

double diffRealSec(TIMECOUNTER* tc) {
    time_t diffsec = difftime(tc->endTime.tv_sec, tc->startTime.tv_sec);
    suseconds_t diffsub = tc->endTime.tv_usec - tc->startTime.tv_usec;
    return diffsec+diffsub*1e-6;
}

void printUsedTime(TIMECOUNTER* tc) {
	printf("UsedTime: %10.10f(sec)\n", diffRealSec(tc));
}

// �X���b�h�ɓn���ڑ���T�[�o���
typedef struct __serverInfo {
	char hostName[NI_MAXHOST];
	char portNum[NI_MAXSERV];
} SERVERINFO;

typedef struct __threadParam {
	SERVERINFO serverInfo;
	TIMECOUNTER connectTime;
	TIMECOUNTER sendRecvTime;
	bool result;	// ����M����
} THREADPARAM;

// ���g���C�A����
#define TRIALNUM 40

// ���d�x�̎w��i�X���b�h���j
#define THREADNUM 25

// �R�l�N�V�������Ƃ�EchoBack�ʐM��
#define ECHOBACKNUM 100

// thread function
void* thread_func(void *arg) {

	THREADPARAM* tp = (THREADPARAM*) arg;
	SERVERINFO* si = &(tp->serverInfo);
	int csocket;

	// �X�^�[�g�w���҂�
	while ( true) {
		if (isRunnable == true) {
			break;
		}
		usleep(1000);	// 1ms
	}

	// �ڑ����Ԍv���J�n
	countStart(&(tp->connectTime));

	// �T�[�o�Ƀ\�P�b�g�ڑ�
	if ((csocket = clientTCPSocket(si->hostName, si->portNum)) == -1) {
		fprintf(stderr, "client_socket():error\n");
		pthread_exit(NULL);
	}

	// �ڑ����Ԍv���I��
	countEnd(&(tp->connectTime));

	// ����M���Ԍv���J�n
	countStart(&(tp->sendRecvTime));

	// ����M����
	tp->result = sendRecvLoop(csocket, ECHOBACKNUM);

	// ����M���Ԍv���I��
	countEnd(&(tp->sendRecvTime));

	// �\�P�b�g�N���[�Y
	close(csocket);

	// tp�͕ʓr���\�[�X�Ǘ����Ă���̂ł����ł͉�����Ȃ�

	pthread_exit(NULL);
}

void printTotalTime(THREADPARAM* tplist[], int tplistNum) {
	double totalConnectTime = 0;
	double totalSendRecvTime = 0;
	int successNum = 0;
	for (int i = 0; i < tplistNum; i++) {
		printf("Thread(%d): \n", i);
		printf("  result: %d\n", tplist[i]->result);
		printf("  connectTime: ");
		printUsedTime(&(tplist[i]->connectTime));
		printf("  sendRecvTime: ");
		printUsedTime(&(tplist[i]->sendRecvTime));
		if (tplist[i]->result == true) {
			successNum++;
			totalConnectTime += diffRealSec(&(tplist[i]->connectTime));
			totalSendRecvTime += diffRealSec(&(tplist[i]->sendRecvTime));
		}
		free(tplist[i]);
	}
	printf("TOTAL: success: %d/%d\n", successNum, tplistNum);
	printf("TOTAL: connectTime: %lf\n", totalConnectTime);
	printf("TOTAL: sendRecvTime: %lf\n", totalSendRecvTime);
}

bool doConnect(const char* hostName, const char* portNum, THREADPARAM* tplist[], int tplistNum) {

	// �}���`�X���b�h�ŃT�[�o�ڑ�
	pthread_t threadId[tplistNum];
	for (int i = 0; i < tplistNum; i++) {
		threadId[i] = NULL;
	}

	for (int i = 0; i < tplistNum; i++) {
		THREADPARAM* tp = (THREADPARAM*) malloc(sizeof(THREADPARAM));
		strcpy(tp->serverInfo.hostName, hostName);
		strcpy(tp->serverInfo.portNum, portNum);
		tp->result = false;
		tplist[i] = tp;

		if (pthread_create(&threadId[i], NULL, thread_func, tp)) {
			perror("pthread_create");
			return false;
		}
		fprintf(stderr, "thread %d created.\n", i);
	}

	// �X���b�h�X�^�[�g
	isRunnable = true;
	fprintf(stderr, "Thread Start!!\n");

	// �X���b�h�I���҂�
	for (int i = 0; i < tplistNum; i++) {
		if (threadId[i] != NULL && pthread_join(threadId[i], NULL)) {
			perror("pthread_join");
			return false;
		}
	}
	return true;
}

bool start(const char* hostName, const char* portNum) {

	int tpNum = THREADNUM*TRIALNUM;
	THREADPARAM* tplist[tpNum];
	for ( int i=0; i<TRIALNUM; i++ ) {
		int pos = i * THREADNUM;
		THREADPARAM** p = tplist + pos;
		bool result = doConnect(hostName, portNum, p, THREADNUM);
		if ( result == false ) {
			return false;
		}
	}

	// �g�[�^���̏��v���Ԃ̌v�Z�E�\��
	// �}���`�R�A���ɂ����Ċe�X���b�h��[�I������-�J�n����]�𑫂����킹�Ă���̂�
	// �����s���Ԃ��傫���Ȃ�ꍇ������i�n�[�h�E�F�A���񏈗��ɂȂ��Ă��邽�߁j
	printTotalTime(tplist, tpNum);
	return true;
}

int main(void) {

	// ���v���Ԍv��
	TIMECOUNTER tc;
	countStart(&tc);

	start("192.168.102.65", "10000");

	// ���v���Ԍv���i�A�v���P�[�V�����̊J�n�X���b�h��[�I������-�J�n����]�̂��ߎ����s���Ԃɋ߂��j
	countEnd(&tc);
	printf("%d connection by %d thread with %d echoback.\n",
	THREADNUM*TRIALNUM, THREADNUM, ECHOBACKNUM);
	printUsedTime(&tc);
	fflush(stdout);

	return EXIT_SUCCESS;
}
