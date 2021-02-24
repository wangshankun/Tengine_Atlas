#ifndef ATLAS800_H
#define ATLAS800_H

#include <map>
#include <iostream>
#include "acl/acl.h"
#include "CommonDataType/CommonDataType.h"
#include "Log/Log.h"
#include "ErrorCode/ErrorCode.h"
#include "FileManager/FileManager.h"
#include "Framework/ModelProcess/ModelProcess.h"
#include "DvppCommon/DvppCommon.h"
#include "ConfigParser/ConfigParser.h"
#include "SingleOpProcess/SingleOpProcess.h"
#include "ResourceManager/ResourceManager.h"

void SetLogLevel(uint32_t level);
int ChangeToDstDir(std::string path);
int InitAcl(std::string acl_cfg);
int ReleaseAcl();
int AclInitial(int deviceId, aclrtContext &context);
int AclDestroy(int deviceId, aclrtContext &context);
void AclRtSetCurrentContext(aclrtContext &context);

class HiInterface {
public:
    HiInterface(std::string configPath);
    ~HiInterface() {};
    void HiDestory();
    APP_ERROR HiInit(std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                                    std::vector<std::shared_ptr<HiTensor>> &outputVec);
    APP_ERROR HiForword(std::vector<std::shared_ptr<HiTensor>> inputVec,
                                    std::vector<std::shared_ptr<HiTensor>> outputVec, size_t dynamicBatchSize = 0);
    APP_ERROR SoftmaxTopkInit(std::shared_ptr<HiTensor>& input_tensor,
                              std::shared_ptr<HiTensor>& output_tensor,
                              int top_k,  int cpu_thread_num);
    APP_ERROR SoftmaxTopkExec(std::shared_ptr<HiTensor>& input_tensor,
                              std::shared_ptr<HiTensor>& output_tensor);
private:
    APP_ERROR SoftmaxTopkExit();

    std::string configPath_;
    std::shared_ptr<ModelProcess> modelProcess_; // model inference object
    uint32_t outside_;
    uint32_t inside_;
    uint32_t top_k_;
    uint32_t cpu_thread_num_;
    float* softmax_out_;
    float* argmax_out_;
};

#endif
