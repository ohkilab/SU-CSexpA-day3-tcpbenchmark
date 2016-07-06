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
 * @brief TCP�N���C�A���g�\�P�b�g�𐶐����T�[�o�ɐڑ����s��
 * @param hostName �z�X�g��
 * @param portNum �|�[�g�ԍ�
 * @return �N���C�A���g�\�P�b�g
 */
int clientTCPSocket(const char *hostName, const char *portNum);

/**
 * @brief ����M���[�v
 * @param soc �N���C�A���g�\�P�b�g
 * @param times ���M��
 * @return ����M����(true) or ���s(false)
 */
bool sendRecvLoop(int sock, int times);

#endif /* LIBCLIENT_H_ */
