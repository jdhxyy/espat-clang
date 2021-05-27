// Copyright 2021-2021 The jdh99 Authors. All rights reserved.
// esp32��esp8266��AT�̼�������
// Authors: jdh99 <jdh821@163.com>

#include "espat.h"

#include "pt.h"
#include "async.h"
#include "tzat.h"
#include "lagan.h"
#include "tzlist.h"
#include "tzmalloc.h"
#include "tztime.h"

#include <string.h>
#include <stdio.h>

// ���ʱʱ��.��λ:ms
#define CMD_TIMEOUT 500

// ����WIFI��ʱʱ��.��λ:s
#define CONNECT_WIFI_TIMEOUT 20

// ��λʱ��.��λ:ms
#define RESET_TIME 1000

// ģ���������ʱ��.��λ:s
#define START_UP_TIME_MAX 1

// ���ͳ�ʱʱ��.��λ:us
#define SEND_TIMEOUT 200000

#pragma pack(1)

// ���͵�Ԫ
typedef struct {
    uint8_t ip[4];
    uint16_t port;
    TZBufferDynamic* buffer;
} tItem;

// ����ͷ
typedef struct {
    uint8_t ip[4];
    uint16_t port;
    int len;
} tRxHead;

// �۲��߻ص�����
typedef struct {
    EspATRxCallback callback;
} tItemObserver;

#pragma pack()

// AT������
static intptr_t handle = 0;

static EspATLoadParam loadParam;
static int mid = -1;

// ���Ͷ���
static intptr_t list = 0;
// �۲��߶���
static intptr_t listObserver = 0;

// �Ƿ��Ѿ���������
static bool isStartProperly = false;
static bool isSendOK = false;

static tRxHead rxHead;

static void dealUrcReceive(uint8_t* bytes, int size);
static void receiveData(TZATRespResult result, uint8_t* bytes, int size);
static void notifyObserver(uint8_t* bytes, int size);
static void dealUrcBusyP(uint8_t* bytes, int size);
static void dealUrcBusyS(uint8_t* bytes, int size);
static void dealUrcWifiDisconnect(uint8_t* bytes, int size);
static void dealUrcSendOK(uint8_t* bytes, int size);
static int poweronTask(void);
static int reset(void);
static int setBaudRate(bool* result);
static int configParam(bool* result);
static int connectWifi(bool* result);
static int sendTask(void);
static void deleteAllNode(void);
static int checkConnectStatus(void);
static TZListNode* createNode(void);
static bool isExistObserver(EspATRxCallback callback);

// EspATLoad ģ������
void EspATLoad(EspATLoadParam* param) {
    if (mid == -1) {
        mid = TZMallocRegister(0, ESPAT_TAG, ESPAT_MALLOC_SIZE);
        if (mid == -1) {
            LE(ESPAT_TAG, "load failed!malloc register failed!");
            return;
        }
    }

    handle = TZATCreate(param->Send, param->IsAllowSend);
    if (handle == 0) {
        LE(ESPAT_TAG, "load failed!create at object failed!");
        return;
    }

    list = TZListCreateList(mid);
    if (list == 0) {
        LE(ESPAT_TAG, "load failed!create list failed");
    }
    listObserver = TZListCreateList(mid);
    if (listObserver == 0) {
        LE(ESPAT_TAG, "load failed!create observer list failed");
    }

    TZATRegisterUrc(handle, "+IPD,", ":", TZ_BUFFER_TINY_LEN, dealUrcReceive);
    TZATRegisterUrc(handle, "busy p", "\r\n", TZ_BUFFER_TINY_LEN, dealUrcBusyP);
    TZATRegisterUrc(handle, "busy s", "\r\n", TZ_BUFFER_TINY_LEN, dealUrcBusyS);
    TZATRegisterUrc(handle, "WIFI DISCONNECT", "\r\n", TZ_BUFFER_TINY_LEN, dealUrcWifiDisconnect);
    TZATRegisterUrc(handle, "SEND OK", "\r\n", TZ_BUFFER_TINY_LEN, dealUrcSendOK);

    AsyncStart(poweronTask, ASYNC_SECOND);
    AsyncStart(sendTask, ASYNC_NO_WAIT);
    AsyncStart(checkConnectStatus, ASYNC_MINUTE);

    loadParam = *param;
}

static void dealUrcReceive(uint8_t* bytes, int size) {
    TZ_UNUSED(size);
    int port = 0;
    int ip[4] = {0};
    static bool mode = false;
    
    // ����ip�������벻����������
    if (mode) {
        sscanf((char*)bytes, "%d,%d.%d.%d.%d,%d", &rxHead.len, &ip[0], &ip[1], &ip[2], &ip[3], &port);
        if (ip[0] == 0 || port == 0) {
            sscanf((char*)bytes, "%d,\"%d.%d.%d.%d\",%d", &rxHead.len, &ip[0], &ip[1], &ip[2], &ip[3], &port);
            if (ip[0] == 0 || port == 0) {
                return;
            }
            mode = false;
        }
    } else {
        sscanf((char*)bytes, "%d,\"%d.%d.%d.%d\",%d", &rxHead.len, &ip[0], &ip[1], &ip[2], &ip[3], &port);
        if (ip[0] == 0 || port == 0) {
            sscanf((char*)bytes, "%d,%d.%d.%d.%d,%d", &rxHead.len, &ip[0], &ip[1], &ip[2], &ip[3], &port);
            if (ip[0] == 0 || port == 0) {
                return;
            }
            mode = true;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        rxHead.ip[i] = (uint8_t)ip[i];
    }
    rxHead.port = (uint16_t)port;
    if (rxHead.len > 0 && rxHead.port != 0) {
        TZATSetWaitDataCallback(handle, rxHead.len, SEND_TIMEOUT, receiveData);
    }
}

static void receiveData(TZATRespResult result, uint8_t* bytes, int size) {
    if (result != TZAT_RESP_RESULT_OK || bytes == 0 || size == 0) {
        LW(ESPAT_TAG, "receive data failed!result:%d ip:%s port:%d len:%d", result, rxHead.ip, rxHead.port, rxHead.len);
        return;
    }
    if (size != rxHead.len) {
        LW(ESPAT_TAG, "receive data failed!len is not match ip:%s port:%d len:%d:%d", result, rxHead.ip, rxHead.port, 
            rxHead.len, size);
        return;
    }
    notifyObserver(bytes, size);
}

static void notifyObserver(uint8_t* bytes, int size) {
    TZListNode* node = TZListGetHeader(listObserver);
    tItemObserver* item = NULL;
    for (;;) {
        if (node == NULL) {
            break;
        }

        item = (tItemObserver*)node->Data;
        if (item->callback) {
            item->callback(bytes, size, rxHead.ip, rxHead.port);
        }

        node = node->Next;
    }
}

static void dealUrcBusyP(uint8_t* bytes, int size) {
    TZ_UNUSED(bytes);
    TZ_UNUSED(size);
    LW(ESPAT_TAG, "busy p...");
}

static void dealUrcBusyS(uint8_t* bytes, int size) {
    TZ_UNUSED(bytes);
    TZ_UNUSED(size);
    LW(ESPAT_TAG, "busy s...");
}

static void dealUrcWifiDisconnect(uint8_t* bytes, int size) {
    TZ_UNUSED(bytes);
    TZ_UNUSED(size);
    LW(ESPAT_TAG, "wifi disconnect");
    isStartProperly = false;
}

static void dealUrcSendOK(uint8_t* bytes, int size) {
    TZ_UNUSED(bytes);
    TZ_UNUSED(size);
    isSendOK = true;
}

static int poweronTask(void) {
    static struct pt pt = {0};
    static bool result = false;

    PT_BEGIN(&pt);

    PT_WAIT_UNTIL(&pt, isStartProperly == false);

    // ��λģ��
    PT_WAIT_THREAD(&pt, reset());
    // �ȴ���������
    ASYNC_WAIT(&pt, START_UP_TIME_MAX * ASYNC_SECOND);
    // ���ò�����
    PT_WAIT_THREAD(&pt, setBaudRate(&result));
    if (result == false) {
        PT_EXIT(&pt);
    }
    
    // ���ò���
    PT_WAIT_THREAD(&pt, configParam(&result));
    if (result == false) {
        PT_EXIT(&pt);
    }

    // ����wifi
    PT_WAIT_THREAD(&pt, connectWifi(&result));
    if (result == false) {
        PT_EXIT(&pt);
    }

    isStartProperly = true;
    PT_END(&pt);
}

static int reset(void) {
    static struct pt pt = {0};
    static uint64_t time = 0;
    static intptr_t respHandle = 0;

    PT_BEGIN(&pt);

    if (loadParam.WriteResetIO != NULL) {
        loadParam.WriteResetIO(false);
        time = TZTimeGet();
        PT_WAIT_UNTIL(&pt, TZTimeGet() - time > RESET_TIME * 1000);
        loadParam.WriteResetIO(true);
    }

    LI(ESPAT_TAG, "soft reset start!");
    respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CMD_TIMEOUT);
    if (respHandle == 0) {
        LE(ESPAT_TAG, "soft reset failed!create resp failed!");
        PT_EXIT(&pt);
    }

    // ��λ
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+RST\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "soft reset failed!no ack!");
    }

    TZATDeleteResp(respHandle);
    PT_END(&pt);
}

static int setBaudRate(bool* result) {
    static struct pt pt = {0};
    static intptr_t respHandle = 0;

    PT_BEGIN(&pt);

    LI(ESPAT_TAG, "set baud rate:%d", loadParam.BaudRate);
    *result = false;
    if (loadParam.SetBaudRate != NULL) {
        loadParam.SetBaudRate(loadParam.BaudRate);
    }

    respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CMD_TIMEOUT);
    if (respHandle == 0) {
        LE(ESPAT_TAG, "set baud rate failed!create resp failed!");
        PT_EXIT(&pt);
    }

    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT\r\n"));
    if (TZATRespGetResult(respHandle) == TZAT_RESP_RESULT_OK) {
        // ͨ�ųɹ�
        LI(ESPAT_TAG, "set baud success");
        TZATDeleteResp(respHandle);
        *result = true;
        PT_EXIT(&pt);
    }

    if (loadParam.BaudRate == ESPAT_BAUD_RATE_DEFAULT || loadParam.SetBaudRate == NULL) {
        LI(ESPAT_TAG, "set baud failed!");
        TZATDeleteResp(respHandle);
        *result = false;
        PT_EXIT(&pt);
    }
    // ����û�з�Ӧ.������Ĭ�ϲ�����,�ٽ��г���
    LI(ESPAT_TAG, "set default baud rate:%d", ESPAT_BAUD_RATE_DEFAULT);
    loadParam.SetBaudRate(ESPAT_BAUD_RATE_DEFAULT);
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT\r\n"));
    if (TZATRespGetResult(respHandle) != TZAT_RESP_RESULT_OK) {
        LE(ESPAT_TAG, "set baud rate failed!no ack!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    // ��Ӧ��.�������������µĲ�����
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+UART_DEF=%d,8,1,0,0\r\n", loadParam.BaudRate));
    if (TZATRespGetResult(respHandle) != TZAT_RESP_RESULT_OK) {
        // ����û�з�Ӧ
        LE(ESPAT_TAG, "set baud rate failed!config baud rate failed!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    // ���ò�����Ϊ���²�����
    LI(ESPAT_TAG, "set baud rate:%d", loadParam.BaudRate);
    loadParam.SetBaudRate(loadParam.BaudRate);
    // ʹ���²������ٴγ���
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT\r\n"));
    if (TZATRespGetResult(respHandle) != TZAT_RESP_RESULT_OK) {
        LE(ESPAT_TAG, "set baud rate failed!use new baud rate no ack!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    TZATDeleteResp(respHandle);
    *result = true;
    PT_END(&pt);
}

static int configParam(bool* result) {
    static struct pt pt = {0};
    static intptr_t respHandle = 0;

    PT_BEGIN(&pt);

    *result = false;
    respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CMD_TIMEOUT);
    if (respHandle == 0) {
        LE(ESPAT_TAG, "config param failed!create resp failed!");
        PT_EXIT(&pt);
    }

    // �رջ���
    LI(ESPAT_TAG, "close echo");
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "ATE0\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "close echo failed!no ack!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }
    
    // �ر��ϵ��Զ�����
    LI(ESPAT_TAG, "stop auto connect");
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CWAUTOCONN=0\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "stop auto connect failed!no ack!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    // ��ʾ�Զ�IP�Ͷ˿�
    LI(ESPAT_TAG, "show dst ip and port");
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CIPDINFO=1\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "show dst ip and port failed!no ack!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    TZATDeleteResp(respHandle);
    *result = true;
    PT_END(&pt);
}

static int connectWifi(bool* result) {
    static struct pt pt = {0};
    static intptr_t respHandle = 0;

    PT_BEGIN(&pt);

    LI(ESPAT_TAG, "connect wifi.ssid:%s", loadParam.WifiSsid);
    *result = false;

    respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CONNECT_WIFI_TIMEOUT * 1000);
    if (respHandle == 0) {
        LE(ESPAT_TAG, "connect wifi failed!create resp failed!");
        PT_EXIT(&pt);
    }

    // ����ģʽ
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CWMODE=1\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "connect wifi failed!set wifi mode failed!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    // ����WIFI
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CWJAP=\"%s\",\"%s\"\r\n", loadParam.WifiSsid, loadParam.WifiPwd));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "connect wifi failed!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }
    
    // ����UDP����
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CIPSTART=\"UDP\",\"0.0.0.0\",1\r\n"));
    if (TZATRespGetLineByKeyword(respHandle, "OK") == false) {
        LE(ESPAT_TAG, "connect wifi failed!.create udp socket failed!");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }
    
    LI(ESPAT_TAG, "connect wifi success!");
    TZATDeleteResp(respHandle);
    *result = true;
    PT_END(&pt);
}

static int sendTask(void) {
    static struct pt pt = {0};
    static TZListNode* node = NULL;
    static tItem* item = NULL;
    static intptr_t respHandle = 0;
    static uint64_t time = 0;
    static char ip[TZ_BUFFER_TINY_LEN] = {0};

    PT_BEGIN(&pt);

    node = TZListGetHeader(list);
    if (node == NULL) {
        PT_EXIT(&pt);
    }
    // ��������ж����������
    if (isStartProperly == false) {
        deleteAllNode();
        PT_EXIT(&pt);
    }

    if (respHandle == 0) {
        respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CMD_TIMEOUT);
        if (respHandle == 0) {
            LE(ESPAT_TAG, "send failed!create resp failed!");
            PT_EXIT(&pt);
        }
    }
    
    if (TZATIsBusy(handle)) {
        PT_EXIT(&pt);
    }

    // ��������
    TZATSetEndSign(handle, '>');
    item = (tItem*)node->Data;
    sprintf(ip, "%d.%d.%d.%d", item->ip[0], item->ip[1], item->ip[2], item->ip[3]);
    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CIPSEND=%d,\"%s\",%d\r\n", item->buffer->len, ip, item->port));
    TZATSetEndSign(handle, '\0');

    if (TZATRespGetResult(respHandle) != TZAT_RESP_RESULT_OK) {
        LE(ESPAT_TAG, "send failed!no ack >");
        TZATDeleteResp(respHandle);
        PT_EXIT(&pt);
    }

    isSendOK = false;
    time = TZTimeGet();
    TZATSendData(handle, item->buffer->buf, item->buffer->len);
    PT_WAIT_UNTIL(&pt, isSendOK || TZTimeGet() - time > SEND_TIMEOUT);
    if (isSendOK == false) {
        LW(ESPAT_TAG, "send failed!timeout!");
    }
    
    // ɾ���ڵ�
    TZFree(item->buffer);
    TZListRemove(list, node);

    PT_END(&pt);
}

static void deleteAllNode(void) {
    TZListNode* node = NULL;
    tItem* item = NULL;
    for (;;) {
        node = TZListGetHeader(list);
        if (node == NULL) {
            break;
        }

        item = (tItem*)node->Data;
        TZFree(item->buffer);
        TZListRemove(list, node);
    }
}

static int checkConnectStatus(void) {
    static struct pt pt = {0};
    static intptr_t respHandle = 0;
    const char* line = NULL;
    int status = 0;

    PT_BEGIN(&pt);

    PT_WAIT_UNTIL(&pt, isStartProperly && TZATIsBusy(handle) == false);

    if (respHandle == 0) {
        respHandle = TZATCreateResp(TZ_BUFFER_LEN, 0, CMD_TIMEOUT);
        if (respHandle == 0) {
            LE(ESPAT_TAG, "check connect status failed!create resp failed!");
            PT_EXIT(&pt);
        }
    }

    PT_WAIT_THREAD(&pt, TZATExecCmd(handle, respHandle, "AT+CIPSTATUS\r\n"));
    if (TZATRespGetResult(respHandle) != TZAT_RESP_RESULT_OK) {
        isStartProperly = false;
        LW(ESPAT_TAG, "connect status offline!no ack!");
        PT_EXIT(&pt);
    }
    line = TZATRespGetLineByKeyword(respHandle, "STATUS:");
    if (line == NULL) {
        isStartProperly = false;
        LW(ESPAT_TAG, "connect status offline!ack is wrong!");
        PT_EXIT(&pt);
    }
    sscanf(line, "STATUS:%d", &status);
    if (status < 2 || status > 4) {
        LW(ESPAT_TAG, "connect status offline!status:%d!", status);
        isStartProperly = false;
    }

    PT_END(&pt);
}

// EspATReceive ��������.�û�ģ����յ����ݺ�����ñ�����
void EspATReceive(uint8_t* data, int size) {
    TZATReceive(handle, data, size);
}

// EspATSend ��������
// ip��Ŀ���ַ,4�ֽ�����
void EspATSend(uint8_t* data, int size, uint8_t* ip, uint16_t port) {
    if (isStartProperly == false) {
        LW(ESPAT_TAG, "esp at send failed!is not start properly!");
        return;
    }

    TZListNode* node = createNode();
    if (node == NULL) {
        LE(ESPAT_TAG, "esp at send failed!create node failed!");
        return;
    }

    tItem* item = (tItem*)node->Data;
    item->buffer = TZMalloc(mid, (int)sizeof(TZBufferDynamic) + size);
    if (item->buffer == NULL) {
        LE(ESPAT_TAG, "esp at send failed!create buffer failed!");
        TZFree(node);
        return;
    }

    memcpy(item->ip, ip, 4);
    item->port = port;
    memcpy(item->buffer->buf, data, (size_t)size);
    item->buffer->len = size;
    TZListAppend(list, node);
}

static TZListNode* createNode(void) {
    TZListNode* node = TZListCreateNode(list);
    if (node == NULL) {
        return NULL;
    }
    node->Data = TZMalloc(mid, sizeof(tItem));
    if (node->Data == NULL) {
        TZFree(node);
        return NULL;
    }
    return node;
}

// EspATRegisterObserver ע����չ۲���
bool EspATRegisterObserver(EspATRxCallback callback) {
    if (listObserver == 0 || callback == NULL) {
        return false;
    }
    if (isExistObserver(callback)) {
        return true;
    }

    TZListNode* node = TZListCreateNode(listObserver);
    if (node == NULL) {
        return false;
    }
    node->Data = TZMalloc(mid, sizeof(tItemObserver));
    if (node->Data == NULL) {
        TZFree(node);
        return false;
    }
    node->Size = sizeof(tItemObserver);

    tItemObserver* item = (tItemObserver*)node->Data;
    item->callback = callback;
    TZListAppend(listObserver, node);
    return true;
}

static bool isExistObserver(EspATRxCallback callback) {
    TZListNode* node = TZListGetHeader(listObserver);
    tItemObserver* item = NULL;
    for (;;) {
        if (node == NULL) {
            break;
        }

        item = (tItemObserver*)node->Data;
        if (item->callback == callback) {
            return true;
        }

        node = node->Next;
    }
    return false;
}

// EspATIsConnectWifi �Ƿ�����wifi�ɹ�
bool EspATIsConnectWifi(void) {
    return isStartProperly;
}

// EspATReboot ����ģ��
void EspATReboot(void) {
    if (isStartProperly) {
        isStartProperly = false;
    }
}
