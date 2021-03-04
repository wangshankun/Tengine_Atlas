#include <iostream>
#include <functional>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <iostream>
#include <functional>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <math.h>
#include "logger.hpp"
#include "node_ops.hpp"
#include "tensor_mem.hpp"
#include "graph.hpp"
#include "operator/atlas.hpp"
#include "tengine_c_api.h"
#include "data_type.hpp"

#include "atlas800_extern.h"
#include <algorithm>


#include <assert.h>

#define gettid() syscall(__NR_gettid)

#define printf(MESSAGE, args...) { \
  const char *A[] = {MESSAGE}; \
  printf("###%s[%s:%d]###",__FILE__,__FUNCTION__,__LINE__); fflush(stdout);\
  if(sizeof(A) > 0) {\
    printf("::"); \
    printf(*A,##args); \
  } else {\
    printf("\n"); \
  }\
}

namespace TEngine
{

namespace AtlasImpl
{

struct AtlasOps : public NodeOps
{
    HiInterface* net;
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;
    int deviceId = 0;
    aclrtContext context = nullptr;
    
    bool Prerun(Node* node)
    {
        AtlasNode* atlas_op = dynamic_cast<AtlasNode*>(node->GetOp());
        AtlasParam* atlas_param = atlas_op->GetParam();
        printf("[AtlasOps]Run: threadID=%ld, pid=%ld\n", gettid(), (long int)getpid());
        std::string config_name = atlas_param->model_name;
        //加载模型
        std::size_t dir_pos  = config_name.find_last_of("/");
        std::string dir_path = config_name.substr(0, dir_pos);

        InitAcl(dir_path + "/acl.json");
        net = new HiInterface(config_name); 
        int ret = net->HiInit(inputTensorVec, outputTensorVec);
        if (ret != 0) {
            printf("Failed to init \r\n");
            return false;
        }

        for (size_t i = 0; i < node->GetInputNum(); ++i)
        {
            const Tensor* input_tensor = node->GetInputTensor(i);
            const TShape& in_shape = input_tensor->GetShape();
            uint8_t* input_rawdata = (uint8_t *)get_tensor_mem(input_tensor);
            auto type_size = DataType::GetTypeSize(input_tensor->GetDataType());
            int in_n = in_shape.GetN();
            int in_c = in_shape.GetC();
            int in_h = in_shape.GetH();
            int in_w = in_shape.GetW();
            int in_size = in_c * in_h * in_w * in_n * type_size;
            assert(in_size  == inputTensorVec[i]->len);
        }

        for(int i = 0; i < node->GetOutputNum(); ++i)
        {
            const Tensor* output_tensor = node->GetOutputTensor(i);
            const TShape& out_shape = output_tensor->GetShape();
            uint8_t* output_rawdata = (uint8_t *)get_tensor_mem(output_tensor);
            auto type_size = DataType::GetTypeSize(output_tensor->GetDataType());
            int out_n = out_shape.GetN();
            int out_c = out_shape.GetC();
            int out_h = out_shape.GetH();
            int out_w = out_shape.GetW();
            int out_size = out_n* out_c * out_h * out_w * type_size;
            assert(out_size == outputTensorVec[i]->len);
        }
        return true;
    }

    bool Run(Node * node)
    {
        for (size_t i = 0; i < node->GetInputNum(); ++i)
        {
            const Tensor* input_tensor = node->GetInputTensor(i);
            uint8_t* input_rawdata = (uint8_t *)get_tensor_mem(input_tensor);
            inputTensorVec[i]->data = input_rawdata;//上个node输入放入NPU推理的inputtensor中
        }

        for(int i = 0; i < node->GetOutputNum(); ++i)
        {
            const Tensor* output_tensor = node->GetOutputTensor(i);
            uint8_t* output_rawdata = (uint8_t *)get_tensor_mem(output_tensor);
            outputTensorVec[i]->data = output_rawdata;//node的输出内存直接赋给npu使用
        }

        net->HiForword(inputTensorVec, outputTensorVec);

        return true;
    }

    bool Postrun(Node* node)
    {   //释放内存，销毁npu资源
        if(net != nullptr)
        {
            net->HiDestory();
        }

        ReleaseAcl();
        return true;
    }
};
}//namespace AtlasImpl

using namespace AtlasImpl;

void RegisterAtlasNodeExec(void)
{
    AtlasOps *ops = new AtlasOps();
    NodeOpsRegistryManager::RegisterOPImplementor(ATLAS_REGISTRY_NAME,
                                                  "Atlas", ops);
}

} //namespace TEngine
