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

#include "ModelProcess.h"
#include "FileManager/FileManager.h"

ModelProcess::ModelProcess()
{

}
ModelProcess::~ModelProcess()
{
    if (!isDeInit_) {
        DeInit();
    }
}
void ModelProcess::DestroyDataset(aclmdlDataset *dataset)
{
    // Just release the DataBuffer object and DataSet object, remain the buffer, because it is managerd by user
    if (dataset != nullptr) {
        for (size_t i = 0; i < aclmdlGetDatasetNumBuffers(dataset); i++) {
            aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(dataset, i);
            if (dataBuffer != nullptr) {
                aclDestroyDataBuffer(dataBuffer);
                dataBuffer = nullptr;
            }
        }
        aclmdlDestroyDataset(dataset);
        dataset = nullptr;
    }
}
aclmdlDesc *ModelProcess::GetModelDesc()
{
    return modelDesc_.get();
}

int ModelProcess::ModelInference(size_t dynamicBatchSize)
{
    return ModelInference(inputBuffers_, inputSizes_, outputBuffers_, outputSizes_, dynamicBatchSize);
}

int ModelProcess::ModelInference(std::vector<void *> &inputBufs, std::vector<size_t> &inputSizes,
    std::vector<void *> &ouputBufs, std::vector<size_t> &outputSizes, size_t dynamicBatchSize)
{
    LogDebug << "ModelProcess:Begin to inference.";

    APP_ERROR ret = aclrtSetCurrentContext(contextModel_);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to set the acl context, ret = " << ret << ".";
        return ret;
    }

    aclmdlDataset *input = nullptr;
    input = CreateAndFillDataset(inputBufs, inputSizes);
    if (input == nullptr) {
        return APP_ERR_COMM_FAILURE;
    }
    ret = 0;
    if (dynamicBatchSize != 0) {
        size_t index;
        ret = aclmdlGetInputIndexByName(modelDesc_.get(), ACL_DYNAMIC_TENSOR_NAME, &index);
        if (ret != ACL_ERROR_NONE) {
            LogError << "aclmdlGetInputIndexByName failed, maybe static model";
            return APP_ERR_COMM_CONNECTION_FAILURE;
        }
        ret = aclmdlSetDynamicBatchSize(modelId_, input, index, dynamicBatchSize);
        if (ret != ACL_ERROR_NONE) {
            LogError << "dynamic batch set failed, modelId_=" << modelId_ << ", input=" << input << ", index=" << index
                     << ", dynamicBatchSize=" << dynamicBatchSize;
            return APP_ERR_COMM_CONNECTION_FAILURE;
        }
        LogDebug << "set dynamicBatchSize successfully, dynamicBatchSize=" << dynamicBatchSize;
    }
    aclmdlDataset *output = nullptr;
    output = CreateAndFillDataset(ouputBufs, outputSizes);
    if (output == nullptr) {
        DestroyDataset(input);
        input = nullptr;
        return APP_ERR_COMM_FAILURE;
    }

    ret = aclmdlExecute(modelId_, input, output);
    if (ret != APP_ERR_OK) {
        LogError << "aclmdlExecute failed, ret[" << ret << "].";
        return ret;
    }
    
    DestroyDataset(input);
    DestroyDataset(output);
    return APP_ERR_OK;
}

int ModelProcess::DeInit()
{
    LogInfo << "ModelProcess:Begin to deinit instance.";
    isDeInit_ = true;
    APP_ERROR ret = aclmdlUnload(modelId_);
    if (ret != APP_ERR_OK) {
        LogError << "aclmdlUnload  failed, ret["<< ret << "].";
        return ret;
    }

    if (modelDevPtr_ != nullptr) {
        ret = aclrtFree(modelDevPtr_);
        if (ret != APP_ERR_OK) {
            LogError << "aclrtFree  failed, ret[" << ret << "].";
            return ret;
        }
        modelDevPtr_ = nullptr;
    }
    if (weightDevPtr_ != nullptr) {
        ret = aclrtFree(weightDevPtr_);
        if (ret != APP_ERR_OK) {
            LogError << "aclrtFree  failed, ret[" << ret << "].";
            return ret;
        }
        weightDevPtr_ = nullptr;
    }
    for (size_t i = 0; i < inputBuffers_.size(); i++) {
        if (inputBuffers_[i] != nullptr) {
            aclrtFree(inputBuffers_[i]);
            inputBuffers_[i] = nullptr;
        }
    }

    for (size_t i = 0; i < outputBuffers_.size(); i++) {
        if (outputBuffers_[i] != nullptr) {
            aclrtFree(outputBuffers_[i]);
            outputBuffers_[i] = nullptr;
        }
    }
    inputSizes_.clear();
    outputSizes_.clear();
    inputNameIndex_.clear();
    outputNameIndex_.clear();

    if (stream_ != nullptr) {
        ret = aclrtDestroyStream(stream_);
        if (ret != APP_ERR_OK) {
            LogError << "Failed to destroy the stream, ret = " << ret << ".";
            return ret;
        }
        stream_ = nullptr;
    }

    if (contextModel_ != nullptr) {
        ret = aclrtDestroyContext(contextModel_); // Destroy context
        if (ret != APP_ERR_OK) {
            LogError << "Failed to destroy context, ret = " << ret << ".";
            return ret;
        }
        contextModel_ = nullptr;
    }

    ret = aclrtResetDevice(deviceId_); // Reset device
    if (ret != APP_ERR_OK) {
        LogError << "Failed to reset device, ret = " << ret << ".";
        return ret;
    }

    LogInfo << "ModelProcess:Finished deinit instance.";

    return APP_ERR_OK;
}

APP_ERROR ModelProcess::Init(ModelInfo modelInfo)
{
    modelName_ = modelInfo.modelName;
    deviceId_  = modelInfo.deviceId;
    APP_ERROR ret = aclrtSetDevice(deviceId_);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to open acl device: " << deviceId_;
        return ret;
    }

    ret = aclrtCreateContext(&contextModel_, deviceId_);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to create acl context1, ret = " << ret << ".";
        return ret;
    }
    LogInfo << "Created contextModel_ for device " << deviceId_ << " successfully";

    ret = aclrtSetCurrentContext(contextModel_);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to set the acl context, ret = " << ret << ".";
        return ret;
    }

    LogInfo << "Created the acl context successfully.";
    ret = aclrtCreateStream(&stream_); // Create stream for application
    if (ret != APP_ERR_OK) {
        LogError << "Failed to create the acl stream, ret = " << ret << ".";
        return ret;
    }
    LogInfo << "Created the acl stream successfully.";

    std::string modelPath = modelInfo.modelPath;
    LogInfo << "ModelProcess:Begin to init instance.";
    int modelSize = 0;
    std::shared_ptr<uint8_t> modelData = nullptr;
    ret = ReadBinaryFile(modelPath, modelData, modelSize);
    if (ret != APP_ERR_OK) {
        LogError << "read model file failed, ret[" << ret << "].";
        return ret;
    }
    ret = aclmdlQuerySizeFromMem(modelData.get(), modelSize, &modelDevPtrSize_, &weightDevPtrSize_);
    if (ret != APP_ERR_OK) {
        LogError << "aclmdlQuerySizeFromMem failed, ret[" << ret << "].";
        return ret;
    }
    LogDebug << "modelDevPtrSize_[" << modelDevPtrSize_ << "], weightDevPtrSize_[" << weightDevPtrSize_ << "].";

    ret = aclrtMalloc(&modelDevPtr_, modelDevPtrSize_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != APP_ERR_OK) {
        LogError << "aclrtMalloc dev_ptr failed, ret[" << ret << "].";
        return ret;
    }
    ret = aclrtMalloc(&weightDevPtr_, weightDevPtrSize_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != APP_ERR_OK) {
        LogError << "aclrtMalloc weight_ptr failed, ret[" << ret << "] (" << GetAppErrCodeInfo(ret) << ").";
        return ret;
    }
    
    ret = aclmdlLoadFromMemWithMem(modelData.get(), modelSize, &modelId_, modelDevPtr_, modelDevPtrSize_,
        weightDevPtr_, weightDevPtrSize_);
    if (ret != APP_ERR_OK) {
        LogError << "aclmdlLoadFromMemWithMem failed, ret[" << ret << "].";
        return ret;
    }
    ret = aclrtGetCurrentContext(&contextModel_);
    if (ret != APP_ERR_OK) {
        LogError << "aclrtMalloc weight_ptr failed, ret[" << ret << "].";
        return ret;
    }
    // get input and output size
    aclmdlDesc *modelDesc = aclmdlCreateDesc();
    if (modelDesc == nullptr) {
        LogError << "aclmdlCreateDesc failed.";
        return ret;
    }
    ret = aclmdlGetDesc(modelDesc, modelId_);
    if (ret != APP_ERR_OK) {
        LogError << "aclmdlGetDesc ret fail, ret:" << ret << ".";
        return ret;
    }
    modelDesc_.reset(modelDesc, aclmdlDestroyDesc);
    return APP_ERR_OK;
}

aclmdlDataset *ModelProcess::CreateAndFillDataset(std::vector<void *> &bufs, std::vector<size_t> &sizes)
{
    APP_ERROR ret = APP_ERR_OK;
    aclmdlDataset *dataset = aclmdlCreateDataset();
    if (dataset == nullptr) {
        LogError << "ACL_ModelInputCreate failed.";
        return nullptr;
    }

    for (size_t i = 0; i < bufs.size(); ++i) {
        aclDataBuffer *data = aclCreateDataBuffer(bufs[i], sizes[i]);
        if (data == nullptr) {
            DestroyDataset(dataset);
            LogError << "aclCreateDataBuffer failed.";
            return nullptr;
        }

        ret = aclmdlAddDatasetBuffer(dataset, data);
        if (ret != APP_ERR_OK) {
            DestroyDataset(dataset);
            LogError << "ACL_ModelInputDataAdd failed, ret[" << ret << "].";
            return nullptr;
        }
    }
    return dataset;
}

APP_ERROR ModelProcess::InputBufferWithSizeMalloc(aclrtMemMallocPolicy policy)
{
    size_t inputNum = aclmdlGetNumInputs(modelDesc_.get());
    LogDebug << modelName_ << "model inputNum is : " << inputNum << ".";
    for (size_t i = 0; i < inputNum; ++i) {
        void *buffer = nullptr;
        // modify size
        size_t size = aclmdlGetInputSizeByIndex(modelDesc_.get(), i);
        const char * name = aclmdlGetInputNameByIndex(modelDesc_.get(), i);
        APP_ERROR ret = aclrtMalloc(&buffer, size, policy);
        if (ret != APP_ERR_OK) {
            LogFatal << modelName_ << "model input aclrtMalloc fail(ret=" << ret
                     << "), buffer=" << buffer << ", size=" << size << ".";
            if (buffer != nullptr) {
                aclrtFree(buffer);
                buffer = nullptr;
            }
            return ret;
        }
        inputBuffers_.push_back(buffer);
        inputSizes_.push_back(size);
        std::string s_name(name);
        inputNameIndex_[s_name] = i;
        LogDebug << modelName_ << "model inputBuffer i=" << i << ", size=" << size << ".";
    }
    return APP_ERR_OK;
}

APP_ERROR ModelProcess::OutputBufferWithSizeMalloc(aclrtMemMallocPolicy policy)
{
    size_t outputNum = aclmdlGetNumOutputs(modelDesc_.get());
    LogDebug << modelName_ << "model outputNum is : " << outputNum << ".";
    for (size_t i = 0; i < outputNum; ++i) {
        void *buffer = nullptr;
        // modify size
        size_t size = aclmdlGetOutputSizeByIndex(modelDesc_.get(), i);
        const char * name = aclmdlGetOutputNameByIndex(modelDesc_.get(), i);
        APP_ERROR ret = aclrtMalloc(&buffer, size, policy);
        if (ret != APP_ERR_OK) {
            LogFatal << modelName_ << "model output aclrtMalloc fail(ret=" << ret
                     << "), buffer=" << buffer << ", size=" << size << ".";
            if (buffer != nullptr) {
                aclrtFree(buffer);
                buffer = nullptr;
            }
            return ret;
        }
        outputBuffers_.push_back(buffer);
        outputSizes_.push_back(size);
        std::string s_name(name);
        outputNameIndex_[s_name] = i;
        LogDebug << modelName_ << "model outputBuffer i=" << i << ", size=" << size << ".";
    }
    return APP_ERR_OK;
}

APP_ERROR ModelProcess::GetHiTensorIO(std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                                      std::vector<std::shared_ptr<HiTensor>> &outputVec)
{
    APP_ERROR ret;
    size_t inputNum = aclmdlGetNumInputs(modelDesc_.get());
    for (size_t i = 0; i < inputNum; i++) {
        aclDataType tmp_dtype= aclmdlGetInputDataType(modelDesc_.get(), i);
        if (tmp_dtype == ACL_DT_UNDEFINED) {
            LogError << "aclmdlGetInputDataType Error";
            return APP_ERR_ACL_FAILURE;
        }

        aclFormat tmp_format= aclmdlGetInputFormat(modelDesc_.get(), i);
        if (tmp_format == ACL_FORMAT_UNDEFINED) {
            LogError << "aclmdlGetInputFormat Error";
            return APP_ERR_ACL_FAILURE;
        }

        aclmdlIODims tmp_dims;
        ret = aclmdlGetInputDims(modelDesc_.get(), i, &tmp_dims);
        if (ret != APP_ERR_OK) {
            LogError << "GetInputDims Error";
            return ret;
        }

        //dimCount之外的维度无效，清零
        for (int i = tmp_dims.dimCount; i < ACL_MAX_DIM_CNT; i++) {
           tmp_dims.dims[i] = 0;
        }

        std::shared_ptr<HiTensor> tmp_tensor  = std::shared_ptr<HiTensor>(new HiTensor);
        tmp_tensor->type =static_cast<int>(tmp_dtype);
        tmp_tensor->format = static_cast<int>(tmp_format);

        if (tmp_format == ACL_FORMAT_NCHW) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->c = tmp_dims.dims[1];
                tmp_tensor->h = tmp_dims.dims[2];
                tmp_tensor->w = tmp_dims.dims[3];
        }
        if (tmp_format == ACL_FORMAT_NHWC) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->h = tmp_dims.dims[1];
                tmp_tensor->w = tmp_dims.dims[2];
                tmp_tensor->c = tmp_dims.dims[3];
        }
        if (tmp_format == ACL_FORMAT_ND) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->c = tmp_dims.dims[1];
                tmp_tensor->h = 0;
                tmp_tensor->w = 0;
        }

        const char * name = aclmdlGetInputNameByIndex(modelDesc_.get(), i);
        std::string s_name(name);
        tmp_tensor->name = s_name;

        tmp_tensor->len  = aclmdlGetInputSizeByIndex(modelDesc_.get(), i);
        tmp_tensor->data = nullptr;//for host buf

        inputVec.push_back(tmp_tensor);
    }

    size_t OutputNum = aclmdlGetNumOutputs(modelDesc_.get());
    for (size_t i = 0; i < OutputNum; i++) {
        aclDataType tmp_dtype= aclmdlGetOutputDataType(modelDesc_.get(), i);
        if (tmp_dtype == ACL_DT_UNDEFINED) {
            LogError << "aclmdlGetOutputDataType Error";
            return APP_ERR_ACL_FAILURE;
        }
        
        aclFormat tmp_format= aclmdlGetOutputFormat(modelDesc_.get(), i);
        if (tmp_format == ACL_FORMAT_UNDEFINED) {
            LogError << "aclmdlGetOutputFormat Error";
            return APP_ERR_ACL_FAILURE;
        }

        aclmdlIODims tmp_dims;
        ret = aclmdlGetOutputDims(modelDesc_.get(), i, &tmp_dims);
        if (ret != APP_ERR_OK) {
            LogError << "GetOutputDims Error";
            return ret;
        }
        //dimCount之外的维度无效，清零
        for (int i = tmp_dims.dimCount; i < ACL_MAX_DIM_CNT; i++) {
           tmp_dims.dims[i] = 0; 
        }

        std::shared_ptr<HiTensor> tmp_tensor  = std::shared_ptr<HiTensor>(new HiTensor);
        tmp_tensor->type   = static_cast<int>(tmp_dtype);
        tmp_tensor->format = static_cast<int>(tmp_format);
        if (tmp_format == ACL_FORMAT_NCHW) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->c = tmp_dims.dims[1];
                tmp_tensor->h = tmp_dims.dims[2];
                tmp_tensor->w = tmp_dims.dims[3];
        }
        if (tmp_format == ACL_FORMAT_NHWC) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->h = tmp_dims.dims[1];
                tmp_tensor->w = tmp_dims.dims[2];
                tmp_tensor->c = tmp_dims.dims[3];
        }
        if (tmp_format == ACL_FORMAT_ND) {
                tmp_tensor->n = tmp_dims.dims[0];
                tmp_tensor->c = tmp_dims.dims[1];
                tmp_tensor->h = 0;
                tmp_tensor->w = 0;
        }

        const char * name = aclmdlGetOutputNameByIndex(modelDesc_.get(), i);
        std::string s_name(name);
        tmp_tensor->name = s_name;

        tmp_tensor->len  = aclmdlGetOutputSizeByIndex(modelDesc_.get(), i);
        tmp_tensor->data = nullptr;//for host buf

        outputVec.push_back(tmp_tensor);
    }

    return APP_ERR_OK;
}
