#include "atlas800.h"
#include "ops.h"
#include <sys/time.h>
#include <thread>
#include <assert.h>

void SetLogLevel(uint32_t level)
{
    if(level == 0)
    {
        AtlasAscendLog::Log::LogDebugOn();
    }
    if(level == 1)
    {
        AtlasAscendLog::Log::LogInfoOn();
    }
    if(level == 2)
    {
        AtlasAscendLog::Log::LogWarnOn();
    }
    if(level == 3)
    {
        AtlasAscendLog::Log::LogErrorOn();
    }
    if(level == 4)
    {
        AtlasAscendLog::Log::LogFatalOn();
    }
    if(level == 5)
    {
        AtlasAscendLog::Log::LogAllOff();
    }
}

int ChangeToDstDir(std::string path)
{
    APP_ERROR ret = ChangeDir(path.c_str()); // Change the directory to the execute file
    if (ret != APP_ERR_OK) {
        LogError << "Failed to change directory to " << path.c_str();
        return ret;
    }
}

int InitAcl(std::string acl_cfg)
{
    APP_ERROR ret = aclInit(acl_cfg.c_str());
    if (ret != APP_ERR_OK) {
        LogError << "Failed to init acl, ret = " << ret;
    }
    return ret;
}

int ReleaseAcl()
{
    APP_ERROR ret = aclFinalize();
    if (ret != APP_ERR_OK) {
        LogError << "Failed to finalize acl, ret = " << ret << ".";
    }
    return ret;
}

APP_ERROR ReadConfig(const std::string configPath, ModelInfo& modelInfo)
{
    ConfigParser configData;
    // Initialize the config parser module
    if (configData.ParseConfig(configPath) != APP_ERR_OK) {
        return APP_ERR_COMM_INIT_FAIL;
    }

    int deviceId;
    APP_ERROR ret = configData.GetIntValue("device_id", deviceId);
    // Convert from string to digital number
    if (ret != APP_ERR_OK) {
        LogError << "Device id is not digit, please check.";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    // Check validity of device id
    if (deviceId < 0) {
        LogError << "deviceId = " << deviceId << " is less than 0, not valid, please check.";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.deviceId = deviceId;

    std::string tmp;
    ret = configData.GetStringValue("model_path", tmp); // Get the model path from config file
    if (ret != APP_ERR_OK) {
        LogError << "ModelPath in the config file is invalid.";
        return ret;
    }
    char absPath[PATH_MAX];
    // Get the absolute path of model file
    if (realpath(tmp.c_str(), absPath) == nullptr) {
        LogError << "Failed to get the real path.";
        return APP_ERR_COMM_NO_EXIST;
    }
    // Check the validity of model path
    int folderExist = access(absPath, R_OK);
    if (folderExist == -1) {
        LogError << "ModelPath " << absPath <<" doesn't exist or read failed." ;
        return APP_ERR_COMM_NO_EXIST;
    }
    modelInfo.modelPath = std::string(absPath); // Set the absolute path of model file
    modelInfo.method = LOAD_FROM_FILE_WITH_MEM; // Set the ModelLoadMethod
    int tmpNum;    
    ret = configData.GetIntValue("model_width", tmpNum); // Get the input width of model from config file
    if (ret != APP_ERR_OK) {
        LogError << "Model width in the config file is invalid.";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.modelWidth = tmpNum;

    ret = configData.GetIntValue("model_height", tmpNum); // Get the input height of model from config file
    if (ret != APP_ERR_OK) {
        LogError << "Model height in the config file is invalid.";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.modelHeight = tmpNum;

    std::string model_name;
    ret = configData.GetStringValue("model_name", model_name); // Get the input height of model from config file
    if (ret != APP_ERR_OK) {
        LogError << "Model Name in the config file is invalid.";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.modelName = model_name;

    return APP_ERR_OK;
}

HiInterface::HiInterface(std::string configPath) 
{
    configPath_ = configPath;
    modelProcess_ = nullptr;
    softmax_out_ = nullptr;
    argmax_out_ = nullptr;
    cpu_thread_num_ = 16;
    outside_ = 0;
    inside_ = 0;
    top_k_ = 0;
    
}


int HiInterface::HiDestory()
{
    APP_ERROR sf_ret  = APP_ERR_OK;
    APP_ERROR net_ret = APP_ERR_OK;
    if(nullptr != softmax_out_ || nullptr != argmax_out_)
    {
        sf_ret = SoftmaxTopkExit();
    }
    // Destroy resources of modelProcess_
    net_ret = modelProcess_->DeInit();//DeInit实现中加入了context的confirm,这个层面不需要再做
    if (sf_ret != APP_ERR_OK || net_ret != APP_ERR_OK) {
        LogError << "HiDestory Failed";
        return -1;
    }
}

APP_ERROR HiInterface::HiInit(std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                                    std::vector<std::shared_ptr<HiTensor>> &outputVec)
{
    APP_ERROR ret;
    ModelInfo modelInfo; // info of input model
    ret = ReadConfig(configPath_, modelInfo);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to Read Setup Config, ret = " << ret << ".";
        return ret;
    }

    // Create model inference object
    if (modelProcess_ == nullptr) {
        modelProcess_ = std::make_shared<ModelProcess>();
    }
    // Initialize ModelProcess module
    ret = modelProcess_->Init(modelInfo);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to initialize the model process module, ret = " << ret << ".";
        return ret;
    }
    //申请输入输出的device侧内存
    ret = modelProcess_->InputBufferWithSizeMalloc();
    if (ret != APP_ERR_OK) {
        LogError << "Failed to execute InputBufferWithSizeMalloc, ret = " << ret << ".";
        return ret;
    }
    ret = modelProcess_->OutputBufferWithSizeMalloc();
    if (ret != APP_ERR_OK) {
        LogError << "Failed to execute OutputBufferWithSizeMalloc, ret = " << ret << ".";
        return ret;
    }
    //获取输入输出tensor信息
    ret = modelProcess_->GetHiTensorIO(inputVec, outputVec);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to execute GetHiTensorIO, ret = " << ret << ".";
        return ret;
    }

    LogInfo << "Initialized the model process module successfully.";
    return APP_ERR_OK;
}


APP_ERROR HiInterface::HiForword(std::vector<std::shared_ptr<HiTensor>> &inputVec,
                                    std::vector<std::shared_ptr<HiTensor>> &outputVec, size_t dynamicBatchSize)
{
    APP_ERROR ret;

    // 有时候HiForword和HiInit不在一个线程中，
    // 因此要在aclrtMemcpy操作之前再次确保modelcontext是同一个
    ret = modelProcess_->ConfirmContext();
    if (ret != APP_ERR_OK) {
        LogError << "Failed to ConfirmContext, ret = " << ret << ".";
        return ret;
    }

    for (int i = 0; i < inputVec.size(); i++)
    {
        uint32_t index =  modelProcess_->inputNameIndex_[inputVec[i]->name];
        assert(inputVec[i]->len == modelProcess_->inputSizes_[index]);

        ret = aclrtMemcpy(modelProcess_->inputBuffers_[index], modelProcess_->inputSizes_[index], 
                          inputVec[i]->data, inputVec[i]->len,
                          ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != APP_ERR_OK) {
            LogError << "Failed to copy memory from host to device, ret = " \
                    << ret << ".";
            return APP_ERR_ACL_FAILURE;
        }
    }

    // Execute classification model
    ret = modelProcess_->ModelInference(dynamicBatchSize);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to execute the classification model, ret = " << ret << ".";
        return ret;
    }

    for (int i = 0; i < outputVec.size(); i++)
    {
        uint32_t index =  modelProcess_->outputNameIndex_[outputVec[i]->name];
        assert(outputVec[i]->len == modelProcess_->outputSizes_[index]);

        ret = aclrtMemcpy(outputVec[i]->data, outputVec[i]->len,
                          modelProcess_->outputBuffers_[index], modelProcess_->outputSizes_[index],
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != APP_ERR_OK) {
            LogError << "Failed to copy memory from device to host, ret = " \
                    << ret << " len: " << outputVec[i]->len << "outsize: " << modelProcess_->outputSizes_[index] << " .";
            return APP_ERR_ACL_FAILURE;
        }
    }

    return APP_ERR_OK;
}

APP_ERROR HiInterface::SoftmaxTopkInit(std::shared_ptr<HiTensor>& input_tensor,
                                       std::shared_ptr<HiTensor>& output_tensor,
                                       int top_k,  int cpu_thread_num)
{
    
    if (input_tensor->format = ACL_FORMAT_NCHW)
    {
        //outside_ =  input_tensor->n * input_tensor->h;//华为tensor维度表示有点问题，这种没有遇到的情况先注释掉
        //inside_  =  input_tensor->w;
    }
    else if (input_tensor->format = ACL_FORMAT_NHWC)
    {
        outside_ =  input_tensor->n * input_tensor->h;
        inside_  =  input_tensor->w;
        output_tensor->h = input_tensor->n;
        output_tensor->n = input_tensor->h;
        output_tensor->w = 2 * top_k;
        output_tensor->len = sizeof(float) * outside_ * 2 * top_k; 
    }
    else
    {
        LogError << "Unsupport Tensor Format.";
        return APP_ERR_ACL_FAILURE;
    }
    
    softmax_out_ = (float*)malloc(input_tensor->len);
    memset(softmax_out_, 0, input_tensor->len);
    argmax_out_ = (float*)malloc(output_tensor->len);
    memset(argmax_out_, 0, output_tensor->len);

    cpu_thread_num_ = cpu_thread_num;
    top_k_          = top_k;

    return APP_ERR_OK;
}

APP_ERROR HiInterface::SoftmaxTopkExit()
{
    free(softmax_out_);
    free(argmax_out_);
    softmax_out_ = nullptr;
    argmax_out_ = nullptr;
    return APP_ERR_OK;
}


APP_ERROR HiInterface::SoftmaxTopkExec(std::shared_ptr<HiTensor>& input_tensor,
                                       std::shared_ptr<HiTensor>& output_tensor)
{
    softmax(reinterpret_cast<const float*>(input_tensor->data), softmax_out_, outside_, inside_, cpu_thread_num_);
    argmax(softmax_out_, argmax_out_, outside_, inside_, top_k_, cpu_thread_num_);
    to_nchw(argmax_out_, reinterpret_cast<float*>(output_tensor->data), output_tensor->n, output_tensor->h, output_tensor->w, cpu_thread_num_);

    return APP_ERR_OK;
}

