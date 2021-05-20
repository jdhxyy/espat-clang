// Copyright 2021-2021 The jdh99 Authors. All rights reserved.
// esp32和esp8266的AT固件驱动包
// Authors: jdh99 <jdh821@163.com>

#ifndef ESPAT_H
#define ESPAT_H

#include "tztype.h"

#define ESPAT_TAG "espat"
#define ESPAT_MALLOC_SIZE 8192

// 默认波特率
#define ESPAT_BAUD_RATE_DEFAULT 115200

// EspATRxCallback 接收回调函数
typedef void (*EspATRxCallback)(uint8_t* data, int size, const char* ip, uint16_t port);

#pragma pack(1)

// EspATSetBaudrate 设置串口波特率
typedef void (*EspATSetBaudrate)(int baudRate);

// TZDrvAir724UGLoadParam 载入参数
typedef struct {
    // 波特率
    int BaudRate;

    // WIFI热点信息
    char WifiSsid[32];
    char WifiPwd[32];

    // 核心网的ip和端口
    char CoreIp[32];
    uint16_t CorePort;

    // API接口
    // 设置复位脚电平.拉低复位.如果不需要可以设置为NULL
    TZWriteIO WriteResetIO;

    // 设置串口波特率.如果不需要可以设置为NULL
    EspATSetBaudrate SetBaudRate;
    // 是否允许发送
    TZIsAllowSendFunc IsAllowSend;
    // 串口发送函数
    TZDataFunc Send;
} EspATLoadParam;

#pragma pack()

// EspATLoad 模块载入
void EspATLoad(EspATLoadParam param);

// EspATReceive 接收数据.用户模块接收到数据后需调用本函数
void EspATReceive(uint8_t* data, int size);

// EspATSend 发送数据
void EspATSend(uint8_t* data, int size, char* ip, uint16_t port);

// EspATRegisterObserver 注册接收观察者
bool EspATRegisterObserver(EspATRxCallback callback);

// EspATIsConnectWifi 是否连接wifi成功
bool EspATIsConnectWifi(void);

// EspATReboot 重启模块
void EspATReboot(void);

#endif
