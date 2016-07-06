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
#include <sys/_timeval.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>

int clientTCPSocket(const char *hostName, const char *portNum) {

	// �A�h���X���̃q���g���[���N���A
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// �A�h���X���̓o�^
	struct addrinfo* res = NULL;
	int errcode = 0;
	if ((errcode = getaddrinfo(hostName, portNum, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo():%s\n", gai_strerror(errcode));
		return (-1);
	}

	// �z�X�g���E�|�[�g�ԍ��ϊ��i�ڑ��ɕK�{�ł͂Ȃ��j
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

	// �\�P�b�g�̐���
	int soc = 0;
	if ((soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol))
			== -1) {
		perror("socket");
		freeaddrinfo(res);
		return (-1);
	}

	// �T�[�o�ɐڑ�
	if (connect(soc, res->ai_addr, res->ai_addrlen) == -1) {
		perror("connect");
		close(soc);
		freeaddrinfo(res);
		return (-1);
	}

	freeaddrinfo(res);
	return (soc);
}

bool sendRecvLoop(int sock, int times) {
	char buf[512];
	const char msg[] = "Hello World!!";

	// select�p�}�X�N�̏�����
	fd_set mask;
	FD_ZERO(&mask);
	FD_SET(sock, &mask);	// �\�P�b�g�̐ݒ�
	int width = sock + 1;

	// �}�X�N��ݒ�
	fd_set ready = mask;

	// �^�C���A�E�g�l�̃Z�b�g
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	// ����M���[�v
	for (int i = 0; i < times; i++) {

		// �T�[�o�֑��M
		ssize_t len;
		if ((len = send(sock, msg, strlen(msg), 0)) == -1) {
			// �G���[����
			perror("send");
			return false;
		}

		switch (select(width, (fd_set *) &ready, NULL, NULL, &timeout)) {
		case -1:
			perror("select");
			return false;
		case 0:
			perror("select timeout");
			return false;
		default:
			if (FD_ISSET(sock, &ready)) {
				// �T�[�o�����M
				if ((len = recv(sock, buf, sizeof(buf), 0)) == -1) {
					// �G���[����
					perror("recv");
					return false;
				}

				if (len == 0) {
					// �T�[�o������R�l�N�V�����ؒf
					fprintf(stderr, "recv:EOF\n");
					return false;
				}
			}

			break;
		}
	}
	return true;
}
