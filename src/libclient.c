/*
 * libclient.c
 *
 *  Created on: 2016/06/13
 *      Author: yasuh
 */

#include "libclient.h"

#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>

int clientTCPSocket(const char *hostName, const char *portNum) {

	// アドレス情報のヒントをゼロクリア
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// アドレス情報の登録
	struct addrinfo* res = NULL;
	int errcode = 0;
	if ((errcode = getaddrinfo(hostName, portNum, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo():%s\n", gai_strerror(errcode));
		return (-1);
	}

	// ホスト名・ポート番号変換（接続に必須ではない）
	char nbuf[NI_MAXHOST];
	char sbuf[NI_MAXSERV];
	if ((errcode = getnameinfo(res->ai_addr, res->ai_addrlen, nbuf,
			sizeof(nbuf), sbuf, sizeof(sbuf),
			NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
		fprintf(stderr, "getnameinfo():%s\n", gai_strerror(errcode));
		freeaddrinfo(res);
		return (-1);
	}
	fprintf(stderr, "addr=%s\n", nbuf);
	fprintf(stderr, "port=%s\n", sbuf);

	// ソケットの生成
	int soc = 0;
	if ((soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol))
			== -1) {
		perror("socket");
		freeaddrinfo(res);
		return (-1);
	}

	// サーバに接続
	if (connect(soc, res->ai_addr, res->ai_addrlen) == -1) {
		const int number = errno;
		perror("connect");
		close(soc);
		freeaddrinfo(res);
		if ( number == ECONNREFUSED ) {
			return -2;
		}
		return (-1);
	}

	freeaddrinfo(res);
	return (soc);
}

bool sendRecvLoop(int sock, int times) {
	char buf[512];
	const char msg[] = "Hello World!!";

	// select用マスクの初期化
	fd_set mask;
	FD_ZERO(&mask);
	FD_SET(sock, &mask);	// ソケットの設定
	int width = sock + 1;

	// タイムアウト値のセット
	struct timeval timeout;
	timeout.tv_sec = 600;
	timeout.tv_usec = 0;

	// 送受信ループ
	for (int i = 0; i < times; i++) {

		// サーバへ送信
		ssize_t len;
		if ((len = send(sock, msg, strlen(msg), 0)) == -1) {
			// エラー処理
			perror("send");
			return false;
		}

		// マスクを設定
		fd_set ready = mask;

		switch (select(width, (fd_set *) &ready, NULL, NULL, &timeout)) {
		case -1:
			perror("select");
			return false;
		case 0:
			perror("select timeout");
			return false;
		default:
			if (FD_ISSET(sock, &ready)) {
				// サーバから受信
				if ((len = recv(sock, buf, sizeof(buf), 0)) == -1) {
					// エラー処理
					char err_msg[256] = "";
					sprintf(err_msg, "recv: %d", i);
					perror(err_msg);
					return false;
				}

				if (len == 0) {
					// サーバ側からコネクション切断
					fprintf(stderr, "recv:EOF\n");
					return false;
				}
			}

			break;
		}
	}
	return true;
}
