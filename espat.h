// Copyright 2021-2021 The jdh99 Authors. All rights reserved.
// esp32��esp8266��AT�̼�������
// Authors: jdh99 <jdh821@163.com>

#ifndef ESPAT_H
#define ESPAT_H

#include "tztype.h"

#define ESPAT_TAG "espat"
#define ESPAT_MALLOC_SIZE 8192

// Ĭ�ϲ�����
#define ESPAT_BAUD_RATE_DEFAULT 115200

// EspATRxCallback ���ջص�����
typedef void (*EspATRxCallback)(uint8_t* data, int size, const char* ip, uint16_t port);

#pragma pack(1)

// EspATSetBaudrate ���ô��ڲ�����
typedef void (*EspATSetBaudrate)(int baudRate);

// TZDrvAir724UGLoadParam �������
typedef struct {
    // ������
    int BaudRate;

    // WIFI�ȵ���Ϣ
    char WifiSsid[32];
    char WifiPwd[32];

    // ��������ip�Ͷ˿�
    char CoreIp[32];
    uint16_t CorePort;

    // API�ӿ�
    // ���ø�λ�ŵ�ƽ.���͸�λ.�������Ҫ��������ΪNULL
    TZWriteIO WriteResetIO;

    // ���ô��ڲ�����.�������Ҫ��������ΪNULL
    EspATSetBaudrate SetBaudRate;
    // �Ƿ�������
    TZIsAllowSendFunc IsAllowSend;
    // ���ڷ��ͺ���
    TZDataFunc Send;
} EspATLoadParam;

#pragma pack()

// EspATLoad ģ������
void EspATLoad(EspATLoadParam param);

// EspATReceive ��������.�û�ģ����յ����ݺ�����ñ�����
void EspATReceive(uint8_t* data, int size);

// EspATSend ��������
void EspATSend(uint8_t* data, int size, char* ip, uint16_t port);

// EspATRegisterObserver ע����չ۲���
bool EspATRegisterObserver(EspATRxCallback callback);

// EspATIsConnectWifi �Ƿ�����wifi�ɹ�
bool EspATIsConnectWifi(void);

// EspATReboot ����ģ��
void EspATReboot(void);

#endif
