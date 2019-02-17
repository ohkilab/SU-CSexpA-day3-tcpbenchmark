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
 * @param times 送信回数
 * @return 送受信成功(true) or 失敗(false)
 */
bool sendRecvLoop(int sock, int times);

#endif /* LIBCLIENT_H_ */
