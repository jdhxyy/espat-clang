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
// ip��Դ��ַ,4�ֽ�����
typedef void (*EspATRxCallback)(uint8_t* data, int size, uint8_t* ip, uint16_t port);

#pragma pack(1)

// EspATSetBaudrate ���ô��ڲ�����
typedef void (*EspATSetBaudrate)(int baudRate);

// EspATLoadParam �������
typedef struct {
    // ������
    int BaudRate;

    // WIFI�ȵ���Ϣ
    char WifiSsid[TZ_BUFFER_TINY_LEN];
    char WifiPwd[TZ_BUFFER_TINY_LEN];

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
void EspATLoad(EspATLoadParam* param);

// EspATReceive ��������.�û�ģ����յ����ݺ�����ñ�����
void EspATReceive(uint8_t* data, int size);

// EspATSend ��������
// ip��Ŀ���ַ,4�ֽ�����
void EspATSend(uint8_t* data, int size, uint8_t* ip, uint16_t port);

// EspATRegisterObserver ע����չ۲���
bool EspATRegisterObserver(EspATRxCallback callback);

// EspATIsConnectWifi �Ƿ�����wifi�ɹ�
bool EspATIsConnectWifi(void);

// EspATReboot ����ģ��
void EspATReboot(void);

#endif
