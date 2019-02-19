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
#include <sys/resource.h>
#include "libclient.h"

// 生成したスレッドを同時スタートさせるためのフラグ
volatile bool isRunnable = false;
volatile bool isPrepared = false;

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

/**
  * amount required time 
  */
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
	int failConnectNum;
	int failSendRecvNum;
} THREADPARAM;

// 総トライアル数
int TRIALNUM = 1;

// 多重度の指定（スレッド数）
int THREADNUM = 1000;

// コネクションごとのEchoBack通信回数
int ECHOBACKNUM = 100;

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
		if (isPrepared == true ) {
			usleep(1000);	// 1ms
		}
		else {
			sleep(1);	// 1sec
		}
	}

	while (tp->result == false) {

		// 接続時間計測開始
		countStart(&(tp->connectTime));

		// サーバにソケット接続
		if ((csocket = clientTCPSocket(si->hostName, si->portNum)) == -1) {
			fprintf(stderr, "client_socket():error\n");
			tp->result = false;
			tp->failConnectNum++;

			// 接続時間計測終了
			countEnd(&(tp->connectTime));
			continue;
		}

		// 接続時間計測終了
		countEnd(&(tp->connectTime));

		// 送受信時間計測開始
		countStart(&(tp->sendRecvTime));

		// 送受信処理
		tp->result = sendRecvLoop(csocket, ECHOBACKNUM);
		if ( tp->result == false ) {
			tp->failSendRecvNum++;
		}

		// 送受信時間計測終了
		countEnd(&(tp->sendRecvTime));

		// ソケットクローズ
		close(csocket);
	}

	// tpは別途リソース管理しているのでここでは解放しない
	pthread_exit(NULL);
}

void printTotalTime(THREADPARAM* tplist[], int tplistNum) {
	double totalConnectTime = 0;
	double totalSendRecvTime = 0;
	int successNum = 0;
	int failConnectNum = 0;
	int failSendRecvNum = 0;
	for (int i = 0; i < tplistNum; i++) {
		printf("Thread(%d): \n", i);
		printf("%s", tplist[i]->result ? "true" : "false");
		printf("  connectTime: ");
		printUsedTime(&(tplist[i]->connectTime));
		printf("  sendRecvTime: ");
		printUsedTime(&(tplist[i]->sendRecvTime));
		if (tplist[i]->result == true) {
			successNum++;
			totalConnectTime += diffRealSec(&(tplist[i]->connectTime));
			totalSendRecvTime += diffRealSec(&(tplist[i]->sendRecvTime));
		}
		failConnectNum += tplist[i]->failConnectNum;
		failSendRecvNum += tplist[i]->failSendRecvNum;
		free(tplist[i]);
	}
	printf("TOTAL: success: %d/%d\n", successNum, tplistNum);
	printf("TOTAL: failConnectNum: %d\n", failConnectNum);
	printf("TOTAL: failSendRecvNum: %d\n", failSendRecvNum);
	printf("Time for success:\n");
	printf("TOTAL: connectTime(total): %lf\n", totalConnectTime);
	printf("TOTAL: connectTime(avg): %lf\n", totalConnectTime/successNum);
	printf("TOTAL: sendRecvTime(total): %lf\n", totalSendRecvTime);
	printf("TOTAL: sendRecvTime(avf): %lf\n", totalSendRecvTime/successNum);
}

bool doConnect(const char* hostName, const char* portNum, THREADPARAM* tplist[], int tplistNum, TIMECOUNTER* tc) {

	// To keep the maximum number of threads, limit the memory size for each thread.
	// In 32 bit OS, 4GB virtual memory limitation restricts the number of threads.
	pthread_attr_t attr;
	size_t stacksize;
	pthread_attr_init(&attr);
	pthread_attr_getstacksize(&attr, &stacksize);
	size_t new_stacksize = stacksize / 4;
	pthread_attr_setstacksize(&attr, new_stacksize);
	printf("Thread stack size = %d bytes \n", new_stacksize);

	// マルチスレッドでサーバ接続
	pthread_t threadId[tplistNum];
	for (int i = 0; i < tplistNum; i++) {
		threadId[i] = -1;
	}

	for (int i = 0; i < tplistNum; i++) {
		THREADPARAM* tp = (THREADPARAM*) malloc(sizeof(THREADPARAM));
		strcpy(tp->serverInfo.hostName, hostName);
		strcpy(tp->serverInfo.portNum, portNum);
		tp->result = false;
		tplist[i] = tp;

		if (pthread_create(&threadId[i], &attr, thread_func, tp)) {
			perror("pthread_create");
			return false;
		}
		fprintf(stderr, "thread %d created.\n", i);
	}

	// スレッドの準備終了
	isPrepared = true;
	fprintf(stderr, "start count down: 2\n");
	sleep(1);	// 1 sec
	fprintf(stderr, "start count down: 1\n");
	sleep(1);	// 1 sec

	// スレッドスタート及び計測開始
	countStart(tc);
	isRunnable = true;
	fprintf(stderr, "Thread Start!!\n");

	// スレッド終了待ち
	for (int i = 0; i < tplistNum; i++) {
		if (threadId[i] != -1 && pthread_join(threadId[i], NULL)) {
			perror("pthread_join");
			
			// 所要時間計測（アプリケーションの開始スレッドの[終了時刻-開始時刻]のため実実行時間に近い）
			countEnd(tc);
			return false;
		}
	}
	// 所要時間計測（アプリケーションの開始スレッドの[終了時刻-開始時刻]のため実実行時間に近い）
	countEnd(tc);
	return true;
}

bool start(const char* hostName, const char* portNum, TIMECOUNTER* tc) {

	int tpNum = THREADNUM*TRIALNUM;
	THREADPARAM* tplist[tpNum];
	for ( int i=0; i<TRIALNUM; i++ ) {
		int pos = i * THREADNUM;
		THREADPARAM** p = tplist + pos;
		bool result = doConnect(hostName, portNum, p, THREADNUM, tc);
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

void update_rlimit(int resource, int soft, int hard) {
	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = 81920;
	rl.rlim_max = 81920;
	if (setrlimit(RLIMIT_NOFILE, &rl ) ==  -1 ) {
		perror("setrlimit");
		exit(-1);
	}
}

int main(int argc, char* argv[]) {

	if ( argc != 3 ) {
		printf("Usage: %s IP_ADDR PORT\n", argv[0]);
		exit(-1);
	}

	char* hostname = argv[1];
	char* port = argv[2];

	printf("hostname = %s, port = %s\n", hostname, port);

	// update system resource limitation
	update_rlimit(RLIMIT_NOFILE, 8192, 8192);	// file discriptor
	update_rlimit(RLIMIT_STACK, RLIM_INFINITY, RLIM_INFINITY);

	TRIALNUM = 1;	// 総トライアル数
	THREADNUM = 1000;	// 多重度の指定（スレッド数）
	ECHOBACKNUM = 100;	// コネクションごとのEchoBack通信回数

	// 所要時間計測
	TIMECOUNTER tc;
	bool result = start(hostname, port, &tc);
	if ( result == false ) {
		fprintf(stderr, "ERROR: cannot complete the benchmark.\n");
	}
	printf("%d connection by %d thread with %d echoback.\n",
	THREADNUM*TRIALNUM, THREADNUM, ECHOBACKNUM);
	printUsedTime(&tc);
	fflush(stdout);

	return EXIT_SUCCESS;
}
