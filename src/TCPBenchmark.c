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
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "libclient.h"

// 生成したスレッドを同時スタートさせるためのフラグ
volatile bool isRunnable = false;
volatile bool isPrepared = false;

typedef struct __timeCounter {
	struct timespec startTs;
	struct timespec endTs;
} TIMECOUNTER;

void countStart(TIMECOUNTER* tc) {
	clock_gettime(CLOCK_REALTIME, &(tc->startTs));
}

void countEnd(TIMECOUNTER* tc) {
	clock_gettime(CLOCK_REALTIME, &(tc->endTs));
}

/**
  * amount required time 
  */
double diffRealSec(TIMECOUNTER* tc) {
	time_t diffsec = difftime(tc->endTs.tv_sec, tc->startTs.tv_sec);
	long diffnanosec = tc->endTs.tv_nsec - tc->startTs.tv_nsec;
	return diffsec + diffnanosec*1e-9;
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

int RECONNECT_MAX = 3000;

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

	while (tp->result == false && tp->failConnectNum < RECONNECT_MAX ) {

		// 接続時間計測開始
		countStart(&(tp->connectTime));

		// サーバにソケット接続
		if ((csocket = clientTCPSocket(si->hostName, si->portNum)) < 0) {
			fprintf(stderr, "client_socket():error\n");
			tp->result = false;
			tp->failConnectNum++;

			// 接続時間計測終了
			countEnd(&(tp->connectTime));

			if ( csocket == -2 ) {
				// connection refused ( maybe server down )
				fprintf(stderr, "maybe server down.\n");
				break;
			}
			continue;
		}

		// 接続時間計測終了
		countEnd(&(tp->connectTime));

		// 実際の送受信処理の計測の前に接続確認を行う
		if ( sendRecvLoop(csocket, 1) == false ) {
			fprintf(stderr, "csocket is not connected.\n");
			tp->result = false;
			tp->failConnectNum++;
			continue;
		}

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

void printResult(THREADPARAM* tplist[], int tplistNum, TIMECOUNTER tc[], int tcNum) {
	double totalConnectTime = 0;
	double maxConnectTime = 0;
	double totalSendRecvTime = 0;
	double maxSendRecvTime = 0;
	int successNum = 0;
	int failConnectNum = 0;
	int failSendRecvNum = 0;
	for (int i = 0; i < tplistNum; i++) {
		printf("Thread(%d): ", i);
		printf("%s\n", tplist[i]->result ? "true" : "false");
		printf("  connectTime: ");
		printUsedTime(&(tplist[i]->connectTime));
		printf("  sendRecvTime: ");
		printUsedTime(&(tplist[i]->sendRecvTime));
		if (tplist[i]->result == true) {
			successNum++;
			double connectTime = diffRealSec(&(tplist[i]->connectTime));
			if ( connectTime > maxConnectTime ) { maxConnectTime = connectTime; }
			double sendRecvTime = diffRealSec(&(tplist[i]->sendRecvTime));
			if ( sendRecvTime > maxSendRecvTime ) { maxSendRecvTime = sendRecvTime; }
			totalConnectTime += connectTime;
			totalSendRecvTime += sendRecvTime;
		}
		failConnectNum += tplist[i]->failConnectNum;
		failSendRecvNum += tplist[i]->failSendRecvNum;
		printf("  failConnectNum = %d\n", tplist[i]->failConnectNum);
		printf("  failSendRecvNum = %d\n", tplist[i]->failSendRecvNum);
		free(tplist[i]);
	}

	char* category[] = {
		"thread",
		"success",
		"failedConnect",
		"failedSendRecv",
		"connectTime(total)",
		"connectTime(average)",
		"connectTime(max)",
		"sendRecvTime(total)",
		"sendRecvTime(average)",
		"sendRecvTime(max)",
		"benchmarkTime(include failed)"
	};

	printf("---------------------------------------------------------\n");
	int categoryNum = sizeof(category)/sizeof(char*); 
	for ( int i=0; i<categoryNum; i++ ) {
		printf("%s", category[i]);
		if ( i < categoryNum - 1 ) {
			printf(",");
		}
	}
	printf("\n");

	printf("%d,", tplistNum);
	printf("%d,", successNum);
	printf("%d,", failConnectNum);
	printf("%d,", failSendRecvNum);
	printf("%lf,", totalConnectTime);
	printf("%lf,", totalConnectTime/successNum);
	printf("%lf,", maxConnectTime);
	printf("%lf,", totalSendRecvTime);
	printf("%lf,", totalSendRecvTime/successNum);
	printf("%lf,", maxSendRecvTime);
	printf("%lf\n", diffRealSec(&tc[0]));
	printf("---------------------------------------------------------\n");
	fflush(stdout);
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
	printf("Thread stack size = %ld bytes \n", new_stacksize);

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
		tp->failConnectNum = 0;
		tp->failSendRecvNum = 0;
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
			
			// 所要時間計測（ベンチマーク全体の所要時間（接続・転送失敗の通信も含む））
			countEnd(tc);
			return false;
		}
	}
	// 所要時間計測（ベンチマーク全体の所要時間（接続・転送失敗の通信も含む））
	countEnd(tc);
	return true;
}

bool start(const char* hostName, const char* portNum) {

	TIMECOUNTER tc[TRIALNUM];
	int tpNum = THREADNUM*TRIALNUM;
	bool result = true;
	THREADPARAM* tplist[tpNum];
	for ( int i=0; i<TRIALNUM; i++ ) {
		int pos = i * THREADNUM;
		THREADPARAM** p = tplist + pos;
		bool r = doConnect(hostName, portNum, p, THREADNUM, &tc[i]);
		if ( r == false ) {
			result = false;
		}
	}

	// トータルの所要時間の計算・表示
	// マルチコア環境において各スレッドの[終了時刻-開始時刻]を足し合わせているので
	// 実実行時間より大きくなる場合がある（ハードウェア並列処理になっているため）
	printResult(tplist, tpNum, tc, TRIALNUM);
	return result;
}

void update_rlimit(int resource, int soft, int hard) {
	struct rlimit rl;
	getrlimit(resource, &rl);
	rl.rlim_cur = soft;
	rl.rlim_max = hard;
	if (setrlimit(resource, &rl ) ==  -1 ) {
		perror("setrlimit");
		exit(-1);
	}
}

int main(int argc, char* argv[]) {
	if ( argc != 6 ) {
		printf("Usage: %s IP_ADDR PORT TRIAL THREAD ECHOBACK\n", argv[0]);
		exit(-1);
	}

	char* hostname = argv[1];
	char* port = argv[2];

	// update system resource limitation
	update_rlimit(RLIMIT_NOFILE, 8192, 8192);	// file discriptor
	update_rlimit(RLIMIT_STACK, RLIM_INFINITY, RLIM_INFINITY);

	TRIALNUM = atoi(argv[3]);	// 総トライアル数
	THREADNUM = atoi(argv[4]);	// 多重度の指定（スレッド数）
	ECHOBACKNUM = atoi(argv[5]);	// コネクションごとのEchoBack通信回数

	// 接続情報を表示
	printf("hostname = %s, port = %s\n", hostname, port);
	printf("%d connection by %d thread with %d echoback.\n", THREADNUM*TRIALNUM, THREADNUM, ECHOBACKNUM);

	// 所要時間計測
	bool result = start(hostname, port);
	if ( result == false ) {
		fprintf(stderr, "ERROR: cannot complete the benchmark.\n");
	}
	return EXIT_SUCCESS;
}
