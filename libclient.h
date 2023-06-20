/*
 * libclient.h
 *
 *  Created on: 2016/06/13
 *      Author: yasuh
 */

#ifndef LIBCLIENT_H_
#define LIBCLIENT_H_

#include <stdbool.h>

/**
 * @brief TCPクライアントソケットを生成しサーバに接続を行う
 * @param hostName ホスト名
 * @param portNum ポート番号
 * @return クライアントソケット
 */
int clientTCPSocket(const char *hostName, const char *portNum);

/**
 * @brief 送受信ループ
 * @param soc クライアントソケット
 * @param msg 送信メッセージ
 * @param msg_size 送信メッセージのサイズ
 * @param times 送信回数
 * @param thread_id デバッグ用（メッセージに組み込むため）
 * @return 送受信成功回数
 */
int sendRecvLoop(int sock, const char* msg, int msg_size, int times, int thread_id, const char* responsePostfix );

#endif /* LIBCLIENT_H_ */
