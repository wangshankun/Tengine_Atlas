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

#ifndef MODELPROCSS_H
#define MODELPROCSS_H

#include <cstdio>
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "acl/acl.h"
#include "CommonDataType/CommonDataType.h"
#include "Log/Log.h"
#include "ErrorCode/ErrorCode.h"

// Class of model inference
class ModelProcess {
public:

    ModelProcess();
    ~ModelProcess();

    int Init(ModelInfo modelInfo);
    int DeInit();

    APP_ERROR InputBufferWithSizeMalloc(aclrtMemMallocPolicy policy = ACL_MEM_MALLOC_HUGE_FIRST);
    APP_ERROR OutputBufferWithSizeMalloc(aclrtMemMallocPolicy policy = ACL_MEM_MALLOC_HUGE_FIRST);
    int ModelInference(std::vector<void *> &inputBufs, std::vector<size_t> &inputSizes, std::vector<void *> &ouputBufs,
        std::vector<size_t> &outputSizes, size_t dynamicBatchSize = 0);
    int ModelInference(size_t dynamicBatchSize = 0);
    aclmdlDesc *GetModelDesc();

    APP_ERROR GetHiTensorIO(std::vector<std::shared_ptr<HiTensor>> &inputVec,
                               std::vector<std::shared_ptr<HiTensor>> &outputVec);

    std::vector<void *> inputBuffers_ = {};
    std::vector<size_t> inputSizes_ = {};
    std::map<std::string, int> inputNameIndex_;
    std::vector<void *> outputBuffers_ = {};
    std::vector<size_t> outputSizes_ = {};
    std::map<std::string, int> outputNameIndex_;

private:
    aclmdlDataset *CreateAndFillDataset(std::vector<void *> &bufs, std::vector<size_t> &sizes);
    void DestroyDataset(aclmdlDataset *dataset);

    int deviceId_ = 0; // Device used
    std::string modelName_ = "";
    uint32_t modelId_ = 0; // Id of import model
    void *modelDevPtr_ = nullptr;
    size_t modelDevPtrSize_ = 0;
    void *weightDevPtr_ = nullptr;
    size_t weightDevPtrSize_ = 0;
    aclrtContext contextModel_ = nullptr;
    aclrtStream stream_ = nullptr;
    std::shared_ptr<aclmdlDesc> modelDesc_ = nullptr;
    bool isDeInit_ = false;
};

#endif
