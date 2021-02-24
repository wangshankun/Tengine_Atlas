/*
 * Copyright(C) 2020. Huawei Technologies Co.,Ltd. All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HDC_H
#define HDC_H

#include <cstdint>
#include <map>
#include <thread>
#include <vector>

#include "ascend_hal.h"

#include "ErrorCode/ErrorCode.h"

using HdcSession = HDC_SESSION;

// to ignored the differences between 32-bit and 64-bit
struct HdcBuffer {
    HdcBuffer(uint64_t dataBuf, uint64_t ctrlBuf, uint64_t datalen, uint64_t ctrllen)
        : dataBuf(dataBuf), ctrlBuf(ctrlBuf), datalen(datalen), ctrllen(ctrllen) {};
    uint64_t dataBuf;
    uint64_t ctrlBuf;
    uint64_t datalen;
    uint64_t ctrllen;
};
#define SIZEOF_HDC_BUFFER sizeof(HdcBuffer)

struct HdcManagement {
    HdcManagement()
        : dataTransSession(nullptr),
          userNormalSendHeader(nullptr),
          userNormalRecvHeader(nullptr),
          fastSendDataBuffer(nullptr),
          fastSendCtrlBuffer(nullptr),
          fastSendDataBufferSzie(0),
          fastSendCtrlBufferSzie(0),
          supportSessServer2C(nullptr),
          supportSessServer2CMsg(nullptr),
          supportSessClient2S(nullptr),
          supportSessClient2SMsg(nullptr),
          recvCtrlBuffer(nullptr) {};
    HdcSession dataTransSession;              // hdc session for data transfering
    struct drvHdcMsg *userNormalSendHeader;   // for user normal send
    struct drvHdcMsg *userNormalRecvHeader;   // for user normal recv
    char *fastSendDataBuffer;                 // for user fast data buffer send
    char *fastSendCtrlBuffer;                 // for user fast ctrl buffer send
    uint32_t fastSendDataBufferSzie;          // for user fast data buffer send
    uint32_t fastSendCtrlBufferSzie;          // for user fast ctrl buffer send
    HdcSession supportSessServer2C;           // communication session for sending data from server to client
    struct drvHdcMsg *supportSessServer2CMsg; // HdcMsg for Server2C session
    HdcSession supportSessClient2S;           // communication session for sending data from client to server
    struct drvHdcMsg *supportSessClient2SMsg; // HdcMsg for Client2S session
    std::shared_ptr<char> recvCtrlBuffer;
};

// a entity of HDC class is a pair of server and clients.
class Hdc {
public:
    Hdc(int deviceId, bool fastModeEnable)
        : deviceId_(deviceId), server_(nullptr), client_(nullptr), fastModeEnable_(fastModeEnable) {};
    ~Hdc();

    APP_ERROR HdcServerCreate(HdcSession &session, uint64_t maxFastDataBufferSize = 10485760,
        uint64_t maxFastCtrlBufferSize = 128);
    APP_ERROR HdcServerCreate(const uint32_t sessionCount, std::vector<HdcSession> &sessions,
        uint64_t maxFastDataBufferSize = 10485760, uint64_t maxFastCtrlBufferSize = 128);
    APP_ERROR HdcClientCreate(HdcSession &session, uint64_t maxFastDataBufferSize = 10485760,
        uint64_t maxFastCtrlBufferSize = 128);
    APP_ERROR HdcClientCreate(const uint32_t sessionCount, std::vector<HdcSession> &sessions,
        uint64_t maxFastDataBufferSize = 10485760, uint64_t maxFastCtrlBufferSize = 128);
    APP_ERROR HdcNormalSendto(const HdcSession &session, char *&sendMessage, const uint32_t dataBufferLength);
    APP_ERROR HdcNormalRecv(const HdcSession &session, char *&recvMessage, uint32_t &recvBufferLength);
    APP_ERROR HdcFastBufferMaclloc(const HdcSession &session, char *&sendDataBuffer, const uint32_t dataBufferLength);
    APP_ERROR HdcFastBufferMaclloc(const HdcSession &session, char *&sendDataBuffer, const uint32_t dataBufferLength,
        char *&sendCtrlBuffer, const uint32_t ctrlBufferLength);
    APP_ERROR HdcFastSendto(const HdcSession &session, const uint32_t dataBufferLength,
        const uint32_t ctrlBufferLength = 0);
    APP_ERROR HdcFastSendto(const HdcSession &session, char *&sendDataMessage, const uint32_t dataBufferLength);
    APP_ERROR HdcFastSendto(const HdcSession &session, char *&sendDataMessage, const uint32_t dataBufferLength,
        char *&sendCtrlMessage, const uint32_t ctrlBufferLength);
    APP_ERROR HdcFastRecv(const HdcSession &session, std::shared_ptr<char> &recvDataMessage,
        uint32_t &dataBufferLength);
    APP_ERROR HdcFastRecv(const HdcSession &session, std::shared_ptr<char> &recvDataMessage, uint32_t &dataBufferLength,
        std::shared_ptr<char> &recvCtrlMessage, uint32_t &ctrlBufferLength);
    APP_ERROR HdcClose(HdcSession &session);

private:
    int deviceId_;
    HDC_SERVER server_;
    HDC_CLIENT client_;
    bool fastModeEnable_;
    int useless_; // for hdc internal use, no meaning
    std::shared_ptr<char> nonePtr_;
    std::map<HdcSession, struct HdcManagement> hdcManagement_; // for maloc and free

    APP_ERROR HdcSourcePrepareForNormalMode(HdcSession &session);
    APP_ERROR HdcSourcePrepareForFastMode(const HdcSession &session, const uint32_t fastDataBufferSize,
        const uint32_t fastCtrlBufferSize);
    APP_ERROR HdcFastModeNormalSesionBuild(HdcSession &newSession, struct drvHdcMsg *&messageHeader);
    APP_ERROR HdcFastBufferMalloc(const HdcSession &session, const uint64_t fastDdataBufferSize,
        const uint64_t fastCtrlBufferSize);
    APP_ERROR HdcNormalSendto(const HdcSession &session, char *&sendMessage, const uint32_t dataBufferLength,
        struct drvHdcMsg *sendMessageHeader);
    APP_ERROR HdcNormalRecv(const HdcSession &session, char *&recvMessage, uint32_t &recvBufferLength,
        struct drvHdcMsg *recvMessageHeader);
    APP_ERROR HdcFastModeSendPreprocessing(const HdcSession &session, const uint32_t dataBufferLength,
        const uint32_t ctrlBufferLength, struct drvHdcMsg *messageHeader, struct drvHdcFastSendMsg &fastSendMsg);
    APP_ERROR HdcFastModeRecvPreprocessing(const HdcSession &session, struct drvHdcMsg *messageHeader,
        std::shared_ptr<char> &recvDataBuffer, std::shared_ptr<char> &recvCtrlBuffer);
    APP_ERROR HdcDynamicFastBufferMalloc(std::shared_ptr<char> &recvDataBuffer, uint32_t recvDataBufferLength,
        std::shared_ptr<char> &recvCtrlBuffer, uint32_t recvCtrlBufferLength);
    APP_ERROR HdcFastModeRecvOverMessage(const HdcSession &session);
    APP_ERROR HdcFastModeSendOverMessage(const HdcSession &session);
    static void HdcFastRecvDataBufferFree(char *&recvDataBuffer);
    static void HdcFastRecvCtrlBufferFree(char *&recvCtrlBuffer);
    void HdcFreeAll();
    void HdcNormalMessageHeader(struct drvHdcMsg *&normalHeader);
    void HdcFastBufferFree(char *&sendDataBuf, char *&sendCtrlBuf);
    void HdcSessionFree(HdcSession &session);
    void HdcClientFree();
    void HdcServerFree();
};
#endif