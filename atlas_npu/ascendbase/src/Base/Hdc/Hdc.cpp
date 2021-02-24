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


#include <chrono>
#include <cstring>

#include "Hdc.h"
#include "Log/Log.h"

const int TIME_1000_MS = 1000;
const int HDC_NORMAL_SEND_MAX_SIZE = 512000; // ~512K
const uint32_t DEFUALT_CTRL_BUFFER_SIZE = 4;
const int SESSION_TIME = 3; // Session multiplier
Hdc::~Hdc()
{
    LogDebug << "hdc start to destructor";
    HdcFreeAll();
}

/*
 * Description: single session pair server create
 * maxFastDataBufferSize is used to set fast data buffer,default value 8M
 * maxFastCtrlBufferSize is used to set fast ctrl buffer,default value 128
 * memFlag:HDC_FLAG_MAP_VA32BIT,dvpp 4G memey;HDC_FLAG_MAP_HUGE:big page map,defual=HDC_FLAG_MAP_HUGE
 * Date: 2020/04/12
 * History:
 * [2020-04-12]:add maxFastDataBufferSize and maxFastCtrlBufferSize for flexible settings
 */
APP_ERROR Hdc::HdcServerCreate(HdcSession &session, uint64_t maxFastDataBufferSize, uint64_t maxFastCtrlBufferSize)
{
    if (client_ != nullptr) {
        LogError << "the client alrady exist, one hdc entity can not have both server and client";
        return APP_ERR_COMM_FAILURE;
    }
    while (true) {
        LogInfo << "start single session server, deviceId=" << deviceId_;
        // non-blocking
        int ret = drvHdcServerCreate(deviceId_, HDC_SERVICE_TYPE_USER1, &server_);
        if (ret != DRV_ERROR_NONE) {
            LogError << "create HDC single session server on device=" << deviceId_ << " fial, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        }
        while (true) {
            // Blocking here, wait to client to connect
            ret = HdcSourcePrepareForNormalMode(session);
            if (ret != APP_ERR_OK) {
                if (ret != APP_ERR_COMM_CONNECTION_FAILURE) {
                    LogWarn << "normal session create fail=" << deviceId_ << ", ret=" << ret;
                }
                break;
            }
            if (fastModeEnable_) {
                ret = HdcSourcePrepareForFastMode(session, maxFastDataBufferSize, maxFastCtrlBufferSize);
                if (ret != APP_ERR_OK) {
                    LogError << "apply for fast mode source fail";
                    break;
                }
            }
            LogInfo << "create server and session successfully";
            return APP_ERR_OK;
        }
        HdcFreeAll();
    }
}

APP_ERROR Hdc::HdcSourcePrepareForNormalMode(HdcSession &session)
{
    int ret = 0;
    if (server_ != nullptr && client_ == nullptr) {
        // server
        ret = drvHdcSessionAccept(server_, &session);
        if (ret != DRV_ERROR_NONE) {
            LogWarn << "server create HDC session fail, deviceId=" << deviceId_ << ", ret=" << ret;
            return APP_ERR_COMM_CONNECTION_FAILURE;
        }
    } else if (server_ == nullptr && client_ != nullptr) {
        // client
        ret = drvHdcSessionConnect(0, deviceId_, client_, &session);
        if (ret != DRV_ERROR_NONE) {
            LogWarn << "client session connect to server fail, ret=" << ret;
            std::this_thread::sleep_for(std::chrono::milliseconds(TIME_1000_MS));
            return APP_ERR_COMM_CONNECTION_FAILURE;
        }
    } else {
        LogError << "unkown mode";
        return APP_ERR_COMM_FAILURE;
    }

    ret = drvHdcSetSessionReference(session);
    if (ret != DRV_ERROR_NONE) {
        LogError << "banding session to process fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }

    hdcManagement_.insert(std::make_pair(session, HdcManagement()));
    hdcManagement_[session].dataTransSession = session;

    ret = drvHdcAllocMsg(session, &hdcManagement_[session].userNormalSendHeader, 1);
    if (ret != DRV_ERROR_NONE) {
        LogError << "alloc normal message head fail, ret=" << ret;
        return APP_ERR_COMM_ALLOC_MEM;
    }
    LogDebug << "malloc userNormalSendHeader succesfully, session=" << session
             << ", userNormalSendHeader=" << hdcManagement_[session].userNormalSendHeader;

    ret = drvHdcAllocMsg(session, &hdcManagement_[session].userNormalRecvHeader, 1);
    if (ret != DRV_ERROR_NONE) {
        LogError << "alloc normal message head fail, ret=" << ret;
        return APP_ERR_COMM_ALLOC_MEM;
    }
    LogDebug << "malloc userNormalRecvHeader succesfully, session=" << session
             << ", userNormalRecvHeader=" << hdcManagement_[session].userNormalRecvHeader;
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcSourcePrepareForFastMode(const HdcSession &session, const uint32_t fastDataBufferSize,
    const uint32_t fastCtrlBufferSize)
{
    int ret = HdcFastBufferMalloc(session, fastDataBufferSize, fastCtrlBufferSize);
    if (ret != APP_ERR_OK) {
        LogError << "malloc for fast buffer fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }

    ret = HdcFastModeNormalSesionBuild(hdcManagement_[session].supportSessServer2C,
        hdcManagement_[session].supportSessServer2CMsg);
    if (ret != APP_ERR_OK) {
        LogError << "fast mode source prepare fail";
        return APP_ERR_COMM_FAILURE;
    }

    ret = HdcFastModeNormalSesionBuild(hdcManagement_[session].supportSessClient2S,
        hdcManagement_[session].supportSessClient2SMsg);
    if (ret != APP_ERR_OK) {
        LogError << "fast mode source prepare fail";
        return APP_ERR_COMM_FAILURE;
    }
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcFastModeNormalSesionBuild(HdcSession &newSession, struct drvHdcMsg *&messageHeader)
{
    int ret = 0;
    if (server_ != nullptr && client_ == nullptr) {
        ret = drvHdcSessionAccept(server_, &newSession);
    } else if (server_ == nullptr && client_ != nullptr) {
        ret = drvHdcSessionConnect(0, deviceId_, client_, &newSession);
    } else {
        LogDebug << "unkown mode";
        return APP_ERR_COMM_FAILURE;
    }
    if (ret != DRV_ERROR_NONE) {
        LogError << "create fast send normal session fail, deviceId=" << deviceId_ << ", ret=" << ret;
        return APP_ERR_COMM_CONNECTION_FAILURE;
    }
    LogDebug << "a new session for fast mode is created, newSession=" << newSession;

    ret = drvHdcSetSessionReference(newSession);
    if (ret != DRV_ERROR_NONE) {
        LogError << "banding session to process fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }

    ret = drvHdcAllocMsg(newSession, &messageHeader, 1);
    if (ret != DRV_ERROR_NONE) {
        LogError << "alloc normal message head fail, ret=" << ret;
        return APP_ERR_COMM_ALLOC_MEM;
    }
    LogDebug << "malloc normalHeader succesfully, session=" << newSession << ", messageHeader=" << messageHeader;
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcFastBufferMalloc(const HdcSession &session, const uint64_t fastDdataBufferSize,
    const uint64_t fastCtrlBufferSize)
{
    char *sendDataBuf =
        static_cast<char *>(drvHdcMallocEx(HDC_MEM_TYPE_TX_DATA, nullptr, 0, fastDdataBufferSize, deviceId_, 0));
    drvHdcDmaMap(HDC_MEM_TYPE_TX_DATA, sendDataBuf, deviceId_);
    char *sendCtrlBuf =
        static_cast<char *>(drvHdcMallocEx(HDC_MEM_TYPE_TX_CTRL, nullptr, 0, fastCtrlBufferSize, deviceId_, 0));
    drvHdcDmaMap(HDC_MEM_TYPE_TX_CTRL, sendCtrlBuf, deviceId_);

    if (sendDataBuf == nullptr || sendCtrlBuf == nullptr) {
        LogError << "malloc hdc fast channel memery fail";
        LogError << "fastDdataBufferSize=" << fastDdataBufferSize << ", fastCtrlBufferSize=" << fastCtrlBufferSize;
        HdcFastBufferFree(sendDataBuf, sendCtrlBuf);
        return APP_ERR_COMM_OUT_OF_MEM;
    }

    hdcManagement_[session].fastSendDataBuffer = sendDataBuf;
    hdcManagement_[session].fastSendCtrlBuffer = sendCtrlBuf;
    hdcManagement_[session].fastSendDataBufferSzie = fastDdataBufferSize;
    hdcManagement_[session].fastSendCtrlBufferSzie = fastCtrlBufferSize;

    LogDebug << "fast buffer malloc successfully, session=" << session
             << ",fastSendDataBuffer=" << static_cast<void *>(hdcManagement_[session].fastSendDataBuffer)
             << ",fastSendCtrlBuffer=" << static_cast<void *>(hdcManagement_[session].fastSendCtrlBuffer)
             << ",fastSendDataBufferSzie=" << hdcManagement_[session].fastSendDataBufferSzie
             << ",fastSendCtrlBufferSzie=" << hdcManagement_[session].fastSendCtrlBufferSzie;
    return APP_ERR_OK;
}

/*
 * Description: multi sessions pair
 * sessionCount：ready to create session num
 * sessions: list of sessions when the application is successful
 * maxFastDataBufferSize: fast mode, max data buffer size
 * maxFastCtrlBufferSize: fast mode, max ctrl buffer size
 */
APP_ERROR Hdc::HdcServerCreate(const uint32_t sessionCount, std::vector<HdcSession> &sessions,
    uint64_t maxFastDataBufferSize, uint64_t maxFastCtrlBufferSize)
{
    if (client_ != nullptr) {
        LogError << "the client alrady exist, one hdc entity can not have both server and client";
        return APP_ERR_COMM_FAILURE;
    }
    while (true) {
        LogInfo << "start multi sessions server, deviceId=" << deviceId_;
        // non-blocking
        int ret = drvHdcServerCreate(deviceId_, HDC_SERVICE_TYPE_USER1, &server_);
        if (ret != DRV_ERROR_NONE) {
            LogWarn << "create HDC multi session server on device=" << deviceId_ << " fail, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        }
        for (uint32_t i = 0; i < sessionCount; ++i) {
            HdcSession session = nullptr;
            // Blocking here, wait to client to connect
            ret = HdcSourcePrepareForNormalMode(session);
            if (ret != APP_ERR_OK) {
                if (ret != APP_ERR_COMM_CONNECTION_FAILURE) {
                    LogWarn << "normal session create fail=" << deviceId_ << ", ret=" << ret;
                }
                break;
            }
            if (fastModeEnable_) {
                ret = HdcSourcePrepareForFastMode(session, maxFastDataBufferSize, maxFastCtrlBufferSize);
                if (ret != APP_ERR_OK) {
                    LogError << "apply for fast mode source fail";
                    break;
                }
            }
            sessions.push_back(session);
        }
        if (ret != APP_ERR_OK) {
            // error process
            HdcFreeAll();
            sessions.clear();
            continue;
        }
        LogInfo << "create server and sessions successfully";
        return APP_ERR_OK;
    }
}

/*
 * Description: single session pair server create
 * maxFastDataBufferSize is used to set fast data buffer,default value 8M
 * maxFastCtrlBufferSize is used to set fast ctrl buffer,default value 128
 */
APP_ERROR Hdc::HdcClientCreate(HdcSession &session, uint64_t maxFastDataBufferSize, uint64_t maxFastCtrlBufferSize)
{
    if (server_ != nullptr) {
        LogError << "the server alrady exist, one hdc entity can not have both server and client";
        return APP_ERR_COMM_FAILURE;
    }
    while (true) {
        LogInfo << "start single session clinet, deviceId=" << deviceId_;
        // non-blocking
        int ret = drvHdcClientCreate(&client_, SESSION_TIME, HDC_SERVICE_TYPE_USER1, 0);
        if (ret != DRV_ERROR_NONE) {
            LogError << "client create fail, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        }
        while (true) {
            // Blocking here, wait to client to connect
            ret = HdcSourcePrepareForNormalMode(session);
            if (ret != APP_ERR_OK) {
                if (ret != APP_ERR_COMM_CONNECTION_FAILURE) {
                    LogWarn << "normal session create fail=" << deviceId_ << ", ret=" << ret;
                }
                break;
            }
            if (fastModeEnable_) {
                ret = HdcSourcePrepareForFastMode(session, maxFastDataBufferSize, maxFastCtrlBufferSize);
                if (ret != APP_ERR_OK) {
                    LogError << "apply for fast mode source fail";
                    break;
                }
            }
            LogInfo << "create client and session successfully";
            return APP_ERR_OK;
        }
        HdcFreeAll();
    }
}

/*
 * Description: multi sessions pair
 * sessionCount：ready to create session num
 * sessions: list of sessions when the application is successful
 * maxFastDataBufferSize: fast mode, max data buffer size
 * maxFastCtrlBufferSize: fast mode, max ctrl buffer size
 */
APP_ERROR Hdc::HdcClientCreate(const uint32_t sessionCount, std::vector<HdcSession> &sessions,
    uint64_t maxFastDataBufferSize, uint64_t maxFastCtrlBufferSize)
{
    if (server_ != nullptr) {
        LogError << "the server alrady exist, one hdc entity can not have both server and client";
        return APP_ERR_COMM_FAILURE;
    }
    while (true) {
        LogInfo << "start multi sessions clinet, deviceId=" << deviceId_;
        // non-blocking
        int ret = drvHdcClientCreate(&client_, sessionCount * SESSION_TIME, HDC_SERVICE_TYPE_USER1, 0);
        if (ret != DRV_ERROR_NONE) {
            LogWarn << "client create fail, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        }
        for (uint32_t i = 0; i < sessionCount; ++i) {
            HdcSession session = nullptr;
            // Blocking here, wait to client to connect
            ret = HdcSourcePrepareForNormalMode(session);
            if (ret != APP_ERR_OK) {
                if (ret != APP_ERR_COMM_CONNECTION_FAILURE) {
                    LogWarn << "normal session create fail=" << deviceId_ << ", ret=" << ret;
                }
                break;
            }
            if (fastModeEnable_) {
                ret = HdcSourcePrepareForFastMode(session, maxFastDataBufferSize, maxFastCtrlBufferSize);
                if (ret != APP_ERR_OK) {
                    LogError << "apply for fast mode source fail";
                    break;
                }
            }
            sessions.push_back(session);
        }
        // error process
        if (ret != APP_ERR_OK) {
            HdcFreeAll();
            sessions.clear();
            continue;
        }
        LogInfo << "create client and sessions successfully";
        return APP_ERR_OK;
    }
}

// normal send
APP_ERROR Hdc::HdcNormalSendto(const HdcSession &session, char *&sendMessage, const uint32_t dataBufferLength)
{
    return HdcNormalSendto(session, sendMessage, dataBufferLength, nullptr);
}

// normal send, For Internal usage
APP_ERROR Hdc::HdcNormalSendto(const HdcSession &session, char *&sendMessage, const uint32_t dataBufferLength,
    struct drvHdcMsg *sendMessageHeader)
{
    if (dataBufferLength > HDC_NORMAL_SEND_MAX_SIZE) {
        LogError << "send message length(" << dataBufferLength << ") is bigger than max size "
                 << HDC_NORMAL_SEND_MAX_SIZE;
        return APP_ERR_COMM_INVALID_POINTER;
    }
    if (sendMessageHeader == nullptr) {
        sendMessageHeader = hdcManagement_[session].userNormalSendHeader;
    }

    int ret = drvHdcReuseMsg(sendMessageHeader);
    if (ret != DRV_ERROR_NONE) {
        LogError << "reuse sendMsgHead fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    ret = drvHdcAddMsgBuffer(sendMessageHeader, sendMessage, dataBufferLength);
    if (ret != DRV_ERROR_NONE) {
        LogError << "add normal buffer information to head fail, session=" << session
                 << ", sendMessageHeader=" << sendMessageHeader << "dataBufferLength=" << dataBufferLength
                 << ", ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    ret = drvHdcSend(session, sendMessageHeader, 0);
    if (ret != DRV_ERROR_NONE) {
        LogError << "normal send buffer information to head fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    return APP_ERR_OK;
}

// It is use to normal recieve message. return recvBuffer and recvBufferLength
APP_ERROR Hdc::HdcNormalRecv(const HdcSession &session, char *&recvMessage, uint32_t &recvBufferLength)
{
    return HdcNormalRecv(session, recvMessage, recvBufferLength, nullptr);
}

// It is use to normal recieve message. return recvBuffer and recvBufferLength, For Internal usage
APP_ERROR Hdc::HdcNormalRecv(const HdcSession &session, char *&recvMessage, uint32_t &recvBufferLength,
    struct drvHdcMsg *recvMessageHeader)
{
    if (recvMessageHeader == nullptr) {
        recvMessageHeader = hdcManagement_[session].userNormalRecvHeader;
    }
    int ret = drvHdcReuseMsg(recvMessageHeader);
    if (ret != DRV_ERROR_NONE) {
        LogError << "reuse recvMsgHead fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    int recvBufCount; // we use only one buffer, so it will always be 1.
    ret = drvHdcRecv(session, recvMessageHeader, recvBufferLength, 0, &recvBufCount);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_SOCKET_CLOSE) {
            LogError << "normal recv message fail, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        } else {
            LogDebug << "maybe want to quit recieving, session=" << session;
            return APP_ERR_COMM_CONNECTION_CLOSE;
        }
    }
    int recvMessageLength = 0;
    ret = drvHdcGetMsgBuffer(recvMessageHeader, 0, &recvMessage, &recvMessageLength);
    if (ret != DRV_ERROR_NONE) {
        LogError << "get recv message from recv message head fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    recvBufferLength = recvMessageLength;
    return APP_ERR_OK;
}

// sendto without one more memery copy, first malloc buffer, then send
APP_ERROR Hdc::HdcFastBufferMaclloc(const HdcSession &session, char *&sendDataBuffer, const uint32_t dataBufferLength)
{
    if (dataBufferLength > hdcManagement_[session].fastSendDataBufferSzie) {
        LogError << "the data buffer length(" << dataBufferLength << ") is out of range "
                 << hdcManagement_[session].fastSendDataBufferSzie;
        sendDataBuffer = nullptr;
        return APP_ERR_COMM_OUT_OF_MEM;
    }
    sendDataBuffer = reinterpret_cast<char *>(hdcManagement_[session].fastSendDataBuffer);
    return APP_ERR_OK;
}

// sendto without one more memery copy, first malloc buffer, then send
APP_ERROR Hdc::HdcFastBufferMaclloc(const HdcSession &session, char *&sendDataBuffer, const uint32_t dataBufferLength,
    char *&sendCtrlBuffer, const uint32_t ctrlBufferLength)
{
    if (dataBufferLength > hdcManagement_[session].fastSendDataBufferSzie ||
        ctrlBufferLength > hdcManagement_[session].fastSendCtrlBufferSzie) {
        LogError << "the data buffer length(" << dataBufferLength << ") is out of range "
                 << hdcManagement_[session].fastSendDataBufferSzie << ", or the ctrl buffer length(" << ctrlBufferLength
                 << ") is out of range " << hdcManagement_[session].fastSendCtrlBufferSzie;
        sendDataBuffer = nullptr;
        sendCtrlBuffer = nullptr;
        return APP_ERR_COMM_OUT_OF_MEM;
    }
    sendDataBuffer = reinterpret_cast<char *>(hdcManagement_[session].fastSendDataBuffer);
    sendCtrlBuffer = reinterpret_cast<char *>(hdcManagement_[session].fastSendCtrlBuffer);
    return APP_ERR_OK;
}

// sendto without one more memery copy, first malloc buffer, then send
// blockFlag show blocking or not, default blocking(value 0), non-blocking is HDC_FLAG_NOWAIT.
APP_ERROR Hdc::HdcFastSendto(const HdcSession &session, const uint32_t dataBufferLength,
    const uint32_t ctrlBufferLength)
{
    if (dataBufferLength == 0) {
        LogError << "send data length is 0";
        return APP_ERR_COMM_FAILURE;
    }
    struct drvHdcFastSendMsg fastSendMsg;
    fastSendMsg.srcDataAddr = reinterpret_cast<uint64_t>(hdcManagement_[session].fastSendDataBuffer);
    fastSendMsg.srcCtrlAddr = reinterpret_cast<uint64_t>(hdcManagement_[session].fastSendCtrlBuffer);
    int ret = 0;
    if (server_ != nullptr && client_ == nullptr) {
        // server send
        ret = HdcFastModeSendPreprocessing(hdcManagement_[session].supportSessServer2C, dataBufferLength,
            ctrlBufferLength, hdcManagement_[session].supportSessServer2CMsg, fastSendMsg);
    } else if (server_ == nullptr && client_ != nullptr) {
        // client send
        ret = HdcFastModeSendPreprocessing(hdcManagement_[session].supportSessClient2S, dataBufferLength,
            ctrlBufferLength, hdcManagement_[session].supportSessClient2SMsg, fastSendMsg);
    } else {
        LogError << "unkown hdc mode";
        return APP_ERR_COMM_FAILURE;
    }
    if (ret != APP_ERR_OK) {
        LogError << "get peer buffer fail, ret" << ret;
        return APP_ERR_COMM_FAILURE;
    }

    ret = drvHdcFastSend(session, fastSendMsg, 0);
    if (ret != DRV_ERROR_NONE) {
        LogError << "fast send message fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    ret = HdcFastModeRecvOverMessage(session);
    if (ret != APP_ERR_OK) {
        LogError << "recv over message from peer fail";
        return APP_ERR_COMM_FAILURE;
    }
    LogDebug << "fast send data Successfully";
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcFastModeSendPreprocessing(const HdcSession &session, const uint32_t dataBufferLength,
    const uint32_t ctrlBufferLength, struct drvHdcMsg *messageHeader, struct drvHdcFastSendMsg &fastSendMsg)
{
    HdcBuffer hdcbuffer(0, 0, dataBufferLength, ctrlBufferLength);
    char *sendMessage = (char *)(&hdcbuffer);
    int ret = HdcNormalSendto(session, sendMessage, SIZEOF_HDC_BUFFER, messageHeader);
    if (ret != APP_ERR_OK) {
        LogError << "send buffer message length to peer fail";
        return APP_ERR_COMM_FAILURE;
    }
    char *recvMessage = nullptr;
    uint32_t recvMessageLength = 0;
    ret = HdcNormalRecv(session, recvMessage, recvMessageLength, messageHeader);
    if (ret != APP_ERR_OK || recvMessageLength != SIZEOF_HDC_BUFFER) {
        LogError << "recv DMA recvMessage info from peer fail, ret=" << ret
                 << ", recvMessageLength=" << recvMessageLength;
        return APP_ERR_COMM_FAILURE;
    }
    auto recvHdcBuffer = (HdcBuffer *)(recvMessage);
    fastSendMsg.dstDataAddr = recvHdcBuffer->dataBuf;
    fastSendMsg.dstCtrlAddr = recvHdcBuffer->ctrlBuf;
    fastSendMsg.dataLen = dataBufferLength;
    fastSendMsg.ctrlLen = ctrlBufferLength;
    LogDebug << std::hex << "get peer fast buffer info, dstDataAddr=" << fastSendMsg.dstDataAddr
             << ", dstCtrlAddr=" << fastSendMsg.dstCtrlAddr << std::dec << ", dataLen=" << fastSendMsg.dataLen
             << ", dataLen=" << fastSendMsg.ctrlLen;
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcFastModeRecvOverMessage(const HdcSession &session)
{
    int ret = 0;
    char *recvMessage = nullptr;
    uint32_t recvMessageLength = 0;
    if (server_ != nullptr && client_ == nullptr) {
        // server send
        ret = HdcNormalRecv(hdcManagement_[session].supportSessServer2C, recvMessage, recvMessageLength,
            hdcManagement_[session].supportSessServer2CMsg);
    } else if (server_ == nullptr && client_ != nullptr) {
        // client send
        ret = HdcNormalRecv(hdcManagement_[session].supportSessClient2S, recvMessage, recvMessageLength,
            hdcManagement_[session].supportSessClient2SMsg);
    } else {
        LogError << "unkown hdc mode";
        return APP_ERR_COMM_FAILURE;
    }
    if (ret != APP_ERR_OK || recvMessageLength != 1) {
        LogError << "recv over message from peer fail";
        return APP_ERR_COMM_FAILURE;
    }
    if (recvMessage[0] == 0xFE) {
        LogDebug << "recv fast send over message sucessfully";
        return APP_ERR_OK;
    } else {
        LogError << "recv unkown Message, fail";
        return APP_ERR_COMM_FAILURE;
    }
}


APP_ERROR Hdc::HdcFastSendto(const HdcSession &session, char *&sendDataMessage, const uint32_t dataBufferLength)
{
    char *sendDataBuffer = nullptr;
    int ret = HdcFastBufferMaclloc(session, sendDataBuffer, dataBufferLength);
    if (ret != APP_ERR_OK) {
        LogError << "get fast send buffer fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }
    std::copy(sendDataMessage, sendDataMessage + dataBufferLength, sendDataBuffer);
    ret = HdcFastSendto(session, dataBufferLength);
    if (ret != APP_ERR_OK) {
        LogError << "fast send message fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }
    return APP_ERR_OK;
}

// blockFlag show blocking or not, default blocking(value 0), non-blocking is HDC_FLAG_NOWAIT.
// here will be one more memry copy
APP_ERROR Hdc::HdcFastSendto(const HdcSession &session, char *&sendDataMessage, const uint32_t dataBufferLength,
    char *&sendCtrlMessage, const uint32_t ctrlBufferLength)
{
    char *sendDataBuffer = nullptr;
    char *sendCtrlBuffer = nullptr;
    int ret = HdcFastBufferMaclloc(session, sendDataBuffer, dataBufferLength, sendCtrlBuffer, ctrlBufferLength);
    if (ret != APP_ERR_OK) {
        LogError << "get fast send buffer fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }
    std::copy(sendDataMessage, sendDataMessage + dataBufferLength, sendDataBuffer);
    std::copy(sendCtrlMessage, sendCtrlMessage + ctrlBufferLength, sendCtrlBuffer);
    ret = HdcFastSendto(session, dataBufferLength, ctrlBufferLength);
    if (ret != APP_ERR_OK) {
        LogError << "fast send message fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }
    return APP_ERR_OK;
}

// first normal recv fast buffer size, then malloc fast buffer, then send buffer info to peer,
// at last recv fast message, only recv data message
APP_ERROR Hdc::HdcFastRecv(const HdcSession &session, std::shared_ptr<char> &recvDataMessage,
    uint32_t &dataBufferLength)
{
    uint32_t ctrlBufferLength = 0;
    return HdcFastRecv(session, recvDataMessage, dataBufferLength, hdcManagement_[session].recvCtrlBuffer,
        ctrlBufferLength);
}

// first normal recv fast buffer size, then malloc fast buffer, then send buffer info to peer,
// at last recv fast message, recv data message and ctrl message
APP_ERROR Hdc::HdcFastRecv(const HdcSession &session, std::shared_ptr<char> &recvDataMessage,
    uint32_t &dataBufferLength, std::shared_ptr<char> &recvCtrlMessage, uint32_t &ctrlBufferLength)
{
    int ret = 0;
    if (server_ != nullptr && client_ == nullptr) {
        // server recv
        ret = HdcFastModeRecvPreprocessing(hdcManagement_[session].supportSessClient2S,
            hdcManagement_[session].supportSessClient2SMsg, recvDataMessage, recvCtrlMessage);
    } else if (server_ == nullptr && client_ != nullptr) {
        // client recv
        ret = HdcFastModeRecvPreprocessing(hdcManagement_[session].supportSessServer2C,
            hdcManagement_[session].supportSessServer2CMsg, recvDataMessage, recvCtrlMessage);
    } else {
        LogError << "unkown hdc mode";
        return APP_ERR_COMM_FAILURE;
    }
    if (ret != APP_ERR_OK) {
        if (ret == APP_ERR_COMM_CONNECTION_CLOSE) {
            LogDebug << "maybe want to quit recieving, session=" << session;
            return APP_ERR_COMM_CONNECTION_CLOSE;
        } else {
            LogError << "get peer buffer fail, ret=" << ret;
            return APP_ERR_COMM_FAILURE;
        }
    }

    struct drvHdcFastRecvMsg fastRecvMsg;
    ret = drvHdcFastRecv(session, &fastRecvMsg, 0);
    if (ret != DRV_ERROR_NONE) {
        LogError << "fast recv data fail, session=" << session << "ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    LogDebug << "recv fast message successfully, session=" << session << std::hex
             << ", dataAddr=" << fastRecvMsg.dataAddr;
    dataBufferLength = fastRecvMsg.dataLen;
    ctrlBufferLength = fastRecvMsg.ctrlLen;
    ret = HdcFastModeSendOverMessage(session);
    if (ret != APP_ERR_OK) {
        LogError << "get peer buffer fail, ret=" << ret;
        return APP_ERR_COMM_FAILURE;
    }
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcFastModeRecvPreprocessing(const HdcSession &session, struct drvHdcMsg *messageHeader,
    std::shared_ptr<char> &recvDataBuffer, std::shared_ptr<char> &recvCtrlBuffer)
{
    char *recvMessage = nullptr;
    uint32_t recvMessageLength = 0;
    int ret = HdcNormalRecv(session, recvMessage, recvMessageLength, messageHeader);
    if (ret != APP_ERR_OK || recvMessageLength != SIZEOF_HDC_BUFFER) {
        if (ret == APP_ERR_COMM_CONNECTION_CLOSE) {
            LogDebug << "maybe want to quit recieving, session=" << session;
            return APP_ERR_COMM_CONNECTION_CLOSE;
        } else {
            LogError << "recv DMA recvMessage info from peer fail";
            return APP_ERR_COMM_FAILURE;
        }
    }
    auto recvHdcBuffer = (HdcBuffer *)(recvMessage);
    ret = HdcDynamicFastBufferMalloc(recvDataBuffer, recvHdcBuffer->datalen, recvCtrlBuffer, recvHdcBuffer->ctrllen);
    if (ret != APP_ERR_OK) {
        LogError << "malloc fast buffer length fail";
        return APP_ERR_COMM_ALLOC_MEM;
    }
    HdcBuffer sendHdcBuffer(reinterpret_cast<uint64_t>(recvDataBuffer.get()),
        reinterpret_cast<uint64_t>(recvCtrlBuffer.get()), recvHdcBuffer->datalen, recvHdcBuffer->ctrllen);
    char *sendMessage = (char *)(&sendHdcBuffer);
    ret = HdcNormalSendto(session, sendMessage, SIZEOF_HDC_BUFFER, messageHeader);
    if (ret != APP_ERR_OK) {
        LogError << "send buffer message length to peer fail";
        return APP_ERR_COMM_FAILURE;
    }
    LogDebug << "reply dynamic DMA buffer info to peer successfully";
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcDynamicFastBufferMalloc(std::shared_ptr<char> &recvDataBuffer, uint32_t recvDataBufferLength,
    std::shared_ptr<char> &recvCtrlBuffer, uint32_t recvCtrlBufferLength)
{
    LogDebug << "ready to malloc fast buffer, recvDataBufferLength=" << recvDataBufferLength
             << ", recvCtrlBufferLength=" << recvCtrlBufferLength;
    char *recvDataBuf =
        static_cast<char *>(drvHdcMallocEx(HDC_MEM_TYPE_RX_DATA, nullptr, 0, recvDataBufferLength, deviceId_, 0));
    uint32_t ctrlBufferLength = std::max(recvCtrlBufferLength, DEFUALT_CTRL_BUFFER_SIZE);
    char *recvCtrlBuf =
        static_cast<char *>(drvHdcMallocEx(HDC_MEM_TYPE_RX_CTRL, nullptr, 0, ctrlBufferLength, deviceId_, 0));
    if (recvDataBuf == nullptr || recvCtrlBuf == nullptr) {
        LogError << "malloc dynamic fast buffer fail, recvDataBuf=" << static_cast<void *>(recvDataBuf)
                 << ", recvCtrlBuf=" << static_cast<void *>(recvCtrlBuf);
        Hdc::HdcFastRecvDataBufferFree(recvDataBuf);
        Hdc::HdcFastRecvCtrlBufferFree(recvCtrlBuf);
        return APP_ERR_COMM_ALLOC_MEM;
    }
    drvHdcDmaMap(HDC_MEM_TYPE_RX_DATA, recvDataBuf, deviceId_);
    recvDataBuffer.reset(recvDataBuf, Hdc::HdcFastRecvDataBufferFree);
    drvHdcDmaMap(HDC_MEM_TYPE_RX_CTRL, recvCtrlBuf, deviceId_);
    recvCtrlBuffer.reset(recvCtrlBuf, Hdc::HdcFastRecvCtrlBufferFree);
    LogDebug << std::hex << "dynamic DMA info, recvDataBuffer=" << static_cast<void *>(recvDataBuffer.get())
             << ", recvCtrlBuffer=" << static_cast<void *>(recvCtrlBuffer.get());
    return APP_ERR_OK;
}


APP_ERROR Hdc::HdcFastModeSendOverMessage(const HdcSession &session)
{
    int ret = 0;
    char overMessage[1] = {(char)0xFE};
    char *message = overMessage;
    if (server_ != nullptr && client_ == nullptr) {
        // server recv
        ret = HdcNormalSendto(hdcManagement_[session].supportSessClient2S, message, 1,
            hdcManagement_[session].supportSessClient2SMsg);
        // server recv
    } else if (server_ == nullptr && client_ != nullptr) {
        // client recv
        ret = HdcNormalSendto(hdcManagement_[session].supportSessServer2C, message, 1,
            hdcManagement_[session].supportSessServer2CMsg);
    } else {
        LogError << "unkown hdc mode";
        return APP_ERR_COMM_FAILURE;
    }
    if (ret != APP_ERR_OK) {
        LogError << "send buffer message length to peer fail";
        return APP_ERR_COMM_FAILURE;
    }
    LogDebug << "send fast send over message successfully";
    return APP_ERR_OK;
}

APP_ERROR Hdc::HdcClose(HdcSession &session)
{
    LogDebug << "close user session=" << session;
    HdcSessionFree(hdcManagement_[session].supportSessServer2C);
    HdcSessionFree(hdcManagement_[session].supportSessClient2S);
    HdcSessionFree(hdcManagement_[session].dataTransSession);
    return APP_ERR_OK;
}

// for shared_ptr free
void Hdc::HdcFastRecvDataBufferFree(char *&recvDataBuffer)
{
    if (recvDataBuffer != nullptr) {
        int ret = drvHdcFreeEx(HDC_MEM_TYPE_RX_DATA, recvDataBuffer);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free recvDataBuffer fail, ret=" << ret;
        } else {
            LogDebug << "free recvDataBuffer successfuly, recvDataBuffer=" << static_cast<void *>(recvDataBuffer);
            recvDataBuffer = nullptr;
        }
    }
}

// for shared_ptr free
void Hdc::HdcFastRecvCtrlBufferFree(char *&recvCtrlBuffer)
{
    if (recvCtrlBuffer != nullptr) {
        int ret = drvHdcFreeEx(HDC_MEM_TYPE_RX_CTRL, recvCtrlBuffer);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free recvCtrlBuffer fail, ret=" << ret;
        } else {
            LogDebug << "free recvCtrlBuffer successfuly, recvCtrlBuffer=" << static_cast<void *>(recvCtrlBuffer);
            recvCtrlBuffer = nullptr;
        }
    }
}

// free all
void Hdc::HdcFreeAll()
{
    for (auto it : hdcManagement_) {
        HdcNormalMessageHeader(it.second.userNormalSendHeader);
        HdcNormalMessageHeader(it.second.userNormalRecvHeader);
        HdcFastBufferFree(it.second.fastSendDataBuffer, it.second.fastSendCtrlBuffer);
        HdcNormalMessageHeader(it.second.supportSessServer2CMsg);
        HdcNormalMessageHeader(it.second.supportSessClient2SMsg);
        HdcSessionFree(it.second.supportSessServer2C);
        HdcSessionFree(it.second.supportSessClient2S);
        HdcSessionFree(it.second.dataTransSession);
        HdcClientFree();
        HdcServerFree();
    }
    hdcManagement_.clear();
}

// to free drvHdcMsg
void Hdc::HdcNormalMessageHeader(struct drvHdcMsg *&normalHeader)
{
    if (normalHeader != nullptr) {
        int ret = drvHdcFreeMsg(normalHeader);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free hdc buffer head fail, ret=" << ret;
        } else {
            LogDebug << "free normal message header successfully, normalHeader=" << normalHeader;
            normalHeader = nullptr;
        }
    }
}

// to free fast buffer
void Hdc::HdcFastBufferFree(char *&sendDataBuf, char *&sendCtrlBuf)
{
    if (sendDataBuf != nullptr) {
        int ret = drvHdcFreeEx(HDC_MEM_TYPE_TX_DATA, sendDataBuf);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free fast sendDataBuf fail, ret=" << ret;
        } else {
            LogDebug << "free fast buffer successfully, sendDataBuf=" << static_cast<void *>(sendDataBuf);
            sendDataBuf = nullptr;
        }
    }
    if (sendCtrlBuf != nullptr) {
        int ret = drvHdcFreeEx(HDC_MEM_TYPE_TX_CTRL, sendCtrlBuf);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free fast sendCtrlBuf fail, ret=" << ret;
        } else {
            LogDebug << "free fast buffer successfully, sendCtrlBuf=" << static_cast<void *>(sendCtrlBuf);
            sendCtrlBuf = nullptr;
        }
    }
}

// to free session
void Hdc::HdcSessionFree(HdcSession &session)
{
    if (session != nullptr) {
        int ret = drvHdcSessionClose(session);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free hdc session fail, session=" << session << ", ret=" << ret;
        } else {
            LogDebug << "close session successfully, session=" << session;
        }
        session = nullptr;
    }
}

void Hdc::HdcClientFree()
{
    if (client_ != nullptr) {
        int ret = drvHdcClientDestroy(client_);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free HDC server fail, deviceId=" << deviceId_ << ", ret=" << ret;
        } else {
            LogDebug << "destroy client successfully, client_=" << client_;
            client_ = nullptr;
        }
    }
}

void Hdc::HdcServerFree()
{
    if (server_ != nullptr) {
        int ret = drvHdcServerDestroy(server_);
        if (ret != DRV_ERROR_NONE) {
            LogError << "free HDC server fail, deviceId=" << deviceId_ << ", ret=" << ret;
        } else {
            LogDebug << "destroy server successfully, server_=" << server_;
            server_ = nullptr;
        }
    }
}