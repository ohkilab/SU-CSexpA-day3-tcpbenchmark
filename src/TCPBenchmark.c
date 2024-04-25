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
#include <math.h>
#include "libclient.h"
#include "timecounter.h"

// 生成したスレッドを同時スタートさせるためのフラグ
volatile bool isRunnable = false;
volatile bool isPrepared = false;

// スレッドに渡す接続先サーバ情報
typedef struct __serverInfo
{
	char hostName[NI_MAXHOST];
	char portNum[NI_MAXSERV];
} SERVERINFO;

typedef struct __threadParam
{
	int id;
	SERVERINFO serverInfo;
	TIMECOUNTER connectTime;
	TIMECOUNTER sendRecvTime;
	bool result; // 再接続を行った上での最終的な送受信成功・不成功
	int failedConnectNum;
	int failedSendRecvLoopNum;
	int failedSendRecvNum;
} THREADPARAM;

// 多重度の指定（スレッド数）
int THREADNUM = 1000;

// コネクションごとのEchoBack通信回数
int ECHOBACKNUM = 100;

// 各EchoBackデータサイズ (Byte)
int ECHOBACKSIZE = 0;

// 再接続上限数
int RECONNECT_MAX = 3000;

// 送信メッセージ
char msg[512 + 1];
int MSGLENGTH = 512 + 1;

// 応答メッセージのポストフィックス期待値
// const char* RESPONSE_POSTFIX=":OK";
const char *RESPONSE_POSTFIX = "";

// thread function
void *thread_func(void *arg)
{

	THREADPARAM *tp = (THREADPARAM *)arg;
	SERVERINFO *si = &(tp->serverInfo);
	int csocket;

	// スタート指示待ち
	while (true)
	{
		if (isRunnable == true)
		{
			break;
		}
		if (isPrepared == true)
		{
			usleep(1000); // 1ms
		}
		else
		{
			sleep(1); // 1sec
		}
	}

	while (tp->result == false && tp->failedConnectNum < RECONNECT_MAX)
	{

		// 接続時間計測開始
		countStart(&(tp->connectTime));

		// サーバにソケット接続
		if ((csocket = clientTCPSocket(si->hostName, si->portNum)) < 0)
		{
			fprintf(stderr, "client_socket():error\n");
			tp->result = false;
			tp->failedConnectNum++;

			// 接続時間計測終了
			countEnd(&(tp->connectTime));

			if (csocket == -2)
			{
				// connection refused ( maybe server down )
				fprintf(stderr, "maybe server down.\n");
				break;
			}
			continue;
		}

		// 接続時間計測終了
		countEnd(&(tp->connectTime));

		// 送受信時間計測開始
		countStart(&(tp->sendRecvTime));

		// 送受信処理
		int successCount = sendRecvLoop(csocket, msg, strlen(msg), ECHOBACKNUM, tp->id, RESPONSE_POSTFIX);
		if (successCount == ECHOBACKNUM)
		{
			tp->result = true;
		}
		else
		{
			tp->failedSendRecvNum += (ECHOBACKNUM - successCount);
			tp->failedSendRecvLoopNum++;
		}

		// 送受信時間計測終了
		countEnd(&(tp->sendRecvTime));

		// ソケットクローズ
		close(csocket);
	}

	// tpは別途リソース管理しているのでここでは解放しない
	pthread_exit(NULL);
}

void printResult(THREADPARAM *tplist[], int tplistNum, TIMECOUNTER tc)
{
	double totalConnectTime = 0;
	double maxConnectTime = 0;
	double totalSendRecvTime = 0;
	double maxSendRecvTime = 0;
	double averageConnectTime = 0;
	double averageSendRecvTime = 0;
	int successNum = 0;
	int failedConnectNum = 0;
	int failedSendRecvLoopNum = 0;
	int failedSendRecvNum = 0;

	for (int i = 0; i < tplistNum; i++)
	{
		printf("Thread(%d): ", i);
		printf("%s\n", tplist[i]->result ? "true" : "false");
		printf("  connectTime: ");
		printUsedTime(&(tplist[i]->connectTime));
		printf("  sendRecvTime: ");
		printUsedTime(&(tplist[i]->sendRecvTime));

		if (tplist[i]->result == true)
		{
			successNum++;
			double connectTime = diffRealSec(&(tplist[i]->connectTime));
			if (connectTime > maxConnectTime)
			{
				maxConnectTime = connectTime;
			}
			double sendRecvTime = diffRealSec(&(tplist[i]->sendRecvTime));
			if (sendRecvTime > maxSendRecvTime)
			{
				maxSendRecvTime = sendRecvTime;
			}
			totalConnectTime += connectTime;
			totalSendRecvTime += sendRecvTime;
		}
		failedConnectNum += tplist[i]->failedConnectNum;
		failedSendRecvLoopNum += tplist[i]->failedSendRecvLoopNum;
		failedSendRecvNum += tplist[i]->failedSendRecvNum;
		printf("  failedConnectNum = %d\n", tplist[i]->failedConnectNum);
		printf("  failedSendRecvLoopNum = %d\n", tplist[i]->failedSendRecvLoopNum);
		printf("  failedSendRecvNum = %d\n", tplist[i]->failedSendRecvNum);
		free(tplist[i]);
	}
	averageConnectTime = totalConnectTime / successNum;
	averageSendRecvTime = totalSendRecvTime / successNum;

	double sampleVarianceConnectTime = 0;
	double sampleVarianceSendRecvTime = 0;
	for (int i = 0; i < tplistNum; i++)
	{
		if (tplist[i]->result == true)
		{
			double connectTime = diffRealSec(&(tplist[i]->connectTime));
			sampleVarianceConnectTime += pow((connectTime - averageConnectTime), 2);
			double sendRecvTime = diffRealSec(&(tplist[i]->sendRecvTime));
			sampleVarianceSendRecvTime += pow((sendRecvTime - averageSendRecvTime), 2);
		}
	}
	sampleVarianceConnectTime /= successNum;
	sampleVarianceSendRecvTime /= successNum;

	char *category[] = {
			"thread",
			"success",
			"failedConnect",
			"failedSendRecvLoop",
			"failedSendRecv",
			"connectTime(total)",
			"connectTime(average)",
			"connectTime(sample variance)",
			"connectTime(max)",
			"sendRecvTime(total)",
			"sendRecvTime(average)",
			"sendRecvTime(sample variance)",
			"sendRecvTime(max)",
			"benchmarkTime(include failed)"};

	printf("---------------------------------------------------------\n");
	int categoryNum = sizeof(category) / sizeof(char *);
	for (int i = 0; i < categoryNum; i++)
	{
		printf("%s", category[i]);
		if (i < categoryNum - 1)
		{
			printf(",");
		}
	}
	printf("\n");

	printf("%d,", tplistNum);
	printf("%d,", successNum);
	printf("%d,", failedConnectNum);
	printf("%d,", failedSendRecvLoopNum);
	printf("%d,", failedSendRecvNum);
	printf("%lf,", totalConnectTime);
	printf("%lf,", averageConnectTime);
	printf("%lf,", sampleVarianceConnectTime);
	printf("%lf,", maxConnectTime);
	printf("%lf,", totalSendRecvTime);
	printf("%lf,", averageSendRecvTime);
	printf("%lf,", sampleVarianceSendRecvTime);
	printf("%lf,", maxSendRecvTime);
	printf("%lf\n", diffRealSec(&tc));

	printf("---------------------------------------------------------\n");
	fflush(stdout);
}

bool doConnect(const char *hostName, const char *portNum, THREADPARAM *tplist[], int tplistNum, TIMECOUNTER *tc)
{

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
	for (int i = 0; i < tplistNum; i++)
	{
		threadId[i] = -1;
	}

	for (int i = 0; i < tplistNum; i++)
	{
		THREADPARAM *tp = (THREADPARAM *)malloc(sizeof(THREADPARAM));
		tp->id = i;
		strcpy(tp->serverInfo.hostName, hostName);
		strcpy(tp->serverInfo.portNum, portNum);
		tp->result = false;
		tp->failedConnectNum = 0;
		tp->failedSendRecvLoopNum = 0;
		tp->failedSendRecvNum = 0;
		tplist[i] = tp;

		if (pthread_create(&threadId[i], &attr, thread_func, tp))
		{
			perror("pthread_create");
			return false;
		}
		fprintf(stderr, "thread %d created.\n", i);
	}

	// スレッドの準備終了
	isPrepared = true;
	fprintf(stderr, "start count down: 2\n");
	sleep(1); // 1 sec
	fprintf(stderr, "start count down: 1\n");
	sleep(1); // 1 sec

	// スレッドスタート及び計測開始
	countStart(tc);
	isRunnable = true;
	fprintf(stderr, "Thread Start!!\n");

	// スレッド終了待ち
	for (int i = 0; i < tplistNum; i++)
	{
		if (threadId[i] != -1 && pthread_join(threadId[i], NULL))
		{
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

bool start(const char *hostName, const char *portNum)
{

	TIMECOUNTER tc;
	bool result = true;
	THREADPARAM *tplist[THREADNUM];
	bool r = doConnect(hostName, portNum, tplist, THREADNUM, &tc);
	if (r == false)
	{
		result = false;
	}

	// トータルの所要時間の計算・表示
	// マルチコア環境において各スレッドの[終了時刻-開始時刻]を足し合わせているので
	// 実実行時間より大きくなる場合がある（ハードウェア並列処理になっているため）
	printResult(tplist, THREADNUM, tc);
	return result;
}

void update_rlimit(int resource, int soft, int hard)
{
	struct rlimit rl;
	getrlimit(resource, &rl);
	rl.rlim_cur = soft;
	rl.rlim_max = hard;
	if (setrlimit(resource, &rl) == -1)
	{
		perror("setrlimit");
		exit(-1);
	}
}

bool create_message(char *arr, int arr_size, int msg_length)
{

	static char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	static bool isInitialized = false;
	if (isInitialized == false)
	{
		srand(time(NULL));
		isInitialized = true;
	}

	if (arr_size < msg_length)
	{
		return false;
	}

	msg[0] = '\0';
	for (int i = 0; i < msg_length; i++)
	{
		msg[i] = charset[rand() % strlen(charset)];
		msg[i + 1] = '\0';
	}
	return true;
}

int main(int argc, char *argv[])
{
	if (argc != 6)
	{
		printf("Usage: %s IP_ADDR PORT THREAD ECHOBACK_SIZE ECHOBACK_NUM\n", argv[0]);
		printf("Caution: ECHOBACK_SIZE (Byte) must be less than 512\n");
		printf("Example: %s 192.168.0.5 10000 500 100 10\n", argv[0]);
		printf("  This program accesses to 192.168.0.5:10000 with 500 threads.\n");
		printf("  Each thread sends 100 bytes of data 10 times.\n");
		exit(-1);
	}

	char *hostname = argv[1];
	char *port = argv[2];

	// update system resource limitation
	update_rlimit(RLIMIT_NOFILE, 8192, 8192); // file discriptor
	update_rlimit(RLIMIT_STACK, (int)RLIM_INFINITY, (int)RLIM_INFINITY);

	THREADNUM = atoi(argv[3]);		// 多重度の指定（スレッド数）
	ECHOBACKSIZE = atoi(argv[4]); // 各EchoBackデータサイズ
	ECHOBACKNUM = atoi(argv[5]);	// コネクションごとのEchoBack通信回数

	if (ECHOBACKSIZE < 1 || ECHOBACKSIZE > 512)
	{
		fprintf(stderr, "ECHOBACKSIZE must be between 1 and 512.\n");
		return -1;
	}

	if (ECHOBACKNUM < 1)
	{
		fprintf(stderr, "ECHOBACKNUM must be more than 0.\n");
		return -1;
	}

	// 送信データ生成
	if (create_message(msg, MSGLENGTH, ECHOBACKSIZE) == false)
	{
		fprintf(stderr, "Too much ECHOBACKSIZE.\n");
		return -1;
	}

	// 接続情報を表示
	printf("hostname = %s, port = %s\n", hostname, port);
	printf("%d thread with %d echoback.\n", THREADNUM, ECHOBACKNUM);
	printf("message size is %d bytes.\n", ECHOBACKSIZE);

	// 所要時間計測
	bool result = start(hostname, port);
	if (result == false)
	{
		fprintf(stderr, "ERROR: cannot complete the benchmark.\n");
	}
	return EXIT_SUCCESS;
}
