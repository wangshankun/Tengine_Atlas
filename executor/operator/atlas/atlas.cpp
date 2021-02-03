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

#define gettid() syscall(__NR_gettid)

namespace TEngine
{

namespace AtlasImpl
{

struct AtlasOps : public NodeOps
{
    bool Prerun(Node* node)
    {
        const Tensor* input_tensor = node->GetInputTensor(0);
        const TShape& in_shape = input_tensor->GetShape();

        int  in_c = in_shape.GetC();
        int  in_h = in_shape.GetH();
        int  in_w = in_shape.GetW();

        size_t input_tensor_size = in_c*in_h*in_w;
        uint8_t* nnie_input_buffer;

        AtlasNode* atlas_op = dynamic_cast<AtlasNode*>(node->GetOp());
        AtlasParam* atlas_param = atlas_op->GetParam();
        std::string model_name = atlas_param->model_name;
        //加载模型

        //模型input output初始化

        //通过node传参数
        //nnie_input_buffer = (uint8_t*)mem_alloc(input_tensor_size * type_size);
        //node->SetAttr("nnie_input_buffer", nnie_input_buffer);
        return true;
    }

    bool Postrun(Node* node)
    {
        //释放内存，销毁npu资源
        return true;
    }


    bool Run(Node * node)
    {
        AtlasNode* atlas_op = dynamic_cast<AtlasNode*>(node->GetOp());
        AtlasParam* atlas_param = atlas_op->GetParam();

        printf("[AtlasOps]Run: threadID=%ld, pid=%ld\n", gettid(), (long int)getpid());
        //uint8_t* nnie_input_buffer = any_cast<uint8_t*>(node->GetAttr("nnie_input_buffer"));
        const Tensor* input_tensor = node->GetInputTensor(0);
        const TShape& in_shape = input_tensor->GetShape();
        int  in_c = in_shape.GetC();
        int  in_h = in_shape.GetH();
        int  in_w = in_shape.GetW();
        const void* input_data = get_tensor_mem(input_tensor);
        // preprocess input_data
        auto type_size = DataType::GetTypeSize(input_tensor->GetDataType());
        int img_size = in_c * in_h * in_w * type_size;
        //memcpy(nnie_input_buffer, input_data, img_size);//把input内容给prerun预申请的npu硬件内存中
        
        //推理

        //把结果给outtensor

        return true;
    }
};

} //namespace AtlasImpl

using namespace AtlasImpl;

void RegisterAtlasNodeExec(void)
{
    AtlasOps *ops = new AtlasOps();
    NodeOpsRegistryManager::RegisterOPImplementor(ATLAS_REGISTRY_NAME,
                                                  "Atlas", ops);
}

} //namespace TEngine
