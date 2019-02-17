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

// 生成したスレッドを同時スタートさせるためのフラグ
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

// スレッドに渡す接続先サーバ情報
typedef struct __serverInfo {
	char hostName[NI_MAXHOST];
	char portNum[NI_MAXSERV];
} SERVERINFO;

typedef struct __threadParam {
	SERVERINFO serverInfo;
	TIMECOUNTER connectTime;
	TIMECOUNTER sendRecvTime;
	bool result;	// 送受信結果
} THREADPARAM;

// 総トライアル数
#define TRIALNUM 40

// 多重度の指定（スレッド数）
#define THREADNUM 25

// コネクションごとのEchoBack通信回数
#define ECHOBACKNUM 100

// thread function
void* thread_func(void *arg) {

	THREADPARAM* tp = (THREADPARAM*) arg;
	SERVERINFO* si = &(tp->serverInfo);
	int csocket;

	// スタート指示待ち
	while ( true) {
		if (isRunnable == true) {
			break;
		}
		usleep(1000);	// 1ms
	}

	// 接続時間計測開始
	countStart(&(tp->connectTime));

	// サーバにソケット接続
	if ((csocket = clientTCPSocket(si->hostName, si->portNum)) == -1) {
		fprintf(stderr, "client_socket():error\n");
		pthread_exit(NULL);
	}

	// 接続時間計測終了
	countEnd(&(tp->connectTime));

	// 送受信時間計測開始
	countStart(&(tp->sendRecvTime));

	// 送受信処理
	tp->result = sendRecvLoop(csocket, ECHOBACKNUM);

	// 送受信時間計測終了
	countEnd(&(tp->sendRecvTime));

	// ソケットクローズ
	close(csocket);

	// tpは別途リソース管理しているのでここでは解放しない

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

	// マルチスレッドでサーバ接続
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

	// スレッドスタート
	isRunnable = true;
	fprintf(stderr, "Thread Start!!\n");

	// スレッド終了待ち
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

	// トータルの所要時間の計算・表示
	// マルチコア環境において各スレッドの[終了時刻-開始時刻]を足し合わせているので
	// 実実行時間より大きくなる場合がある（ハードウェア並列処理になっているため）
	printTotalTime(tplist, tpNum);
	return true;
}

int main(void) {

	// 所要時間計測
	TIMECOUNTER tc;
	countStart(&tc);

	start("localhost", "10000");

	// 所要時間計測（アプリケーションの開始スレッドの[終了時刻-開始時刻]のため実実行時間に近い）
	countEnd(&tc);
	printf("%d connection by %d thread with %d echoback.\n",
	THREADNUM*TRIALNUM, THREADNUM, ECHOBACKNUM);
	printUsedTime(&tc);
	fflush(stdout);

	return EXIT_SUCCESS;
}
