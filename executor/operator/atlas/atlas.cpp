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

#include <algorithm>
#include <assert.h>
#include <dlfcn.h>
#include "config_parser.h"

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

using namespace std;

typedef enum {
    HI_ACL_DT_UNDEFINED = -1,
    HI_ACL_FLOAT = 0,
    HI_ACL_FLOAT16 = 1,
    HI_ACL_INT8 = 2,
    HI_ACL_INT32 = 3,
    HI_ACL_UINT8 = 4,
    HI_ACL_INT16 = 6,
    HI_ACL_UINT16 = 7,
    HI_ACL_UINT32 = 8,
    HI_ACL_INT64 = 9,
    HI_ACL_UINT64 = 10,
    HI_ACL_DOUBLE = 11,
    HI_ACL_BOOL = 12,
} hiAclDataType;

//from acl/acl_base.h aclFormat
typedef enum {
    HI_ACL_FORMAT_UNDEFINED = -1,
    HI_ACL_FORMAT_NCHW = 0,
    HI_ACL_FORMAT_NHWC = 1,
    HI_ACL_FORMAT_ND = 2,
    HI_ACL_FORMAT_NC1HWC0 = 3,
    HI_ACL_FORMAT_FRACTAL_Z = 4,
    HI_ACL_FORMAT_FRACTAL_NZ = 29,
} hiAclFormat;

struct HiTensor {
    uint32_t      n;
    uint32_t      c;
    uint32_t      h;
    uint32_t      w;
    hiAclDataType type;     //aclDataType
    hiAclFormat   format;   //aclFormat
    uint32_t      len;
    std::string   name;
    uint8_t*      data;
};


class RunTime
{
public:
    RunTime() {}
    virtual ~RunTime() {}

    virtual int Preprocess(string configName,
                           vector<shared_ptr<HiTensor>> &inputVec,
                           vector<shared_ptr<HiTensor>> &outputVec) = 0;

    virtual int Forword(vector<shared_ptr<HiTensor>> &inputVec,
                        vector<shared_ptr<HiTensor>> &outputVec) = 0;

    virtual int Postprocess() = 0;
};

typedef RunTime* (*creator_exec)();

namespace TEngine
{

namespace AtlasImpl
{

struct AtlasOps : public NodeOps
{
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;
    RunTime *exec = NULL; 
    bool Prerun(Node* node)
    {
        AtlasNode* atlas_op = dynamic_cast<AtlasNode*>(node->GetOp());
        AtlasParam* atlas_param = atlas_op->GetParam();
        printf("[AtlasOps]Run: threadID=%ld, pid=%ld\n", gettid(), (long int)getpid());
        std::string config_path = atlas_param->model_name;

	ConfigParser configData;
        if (configData.ParseConfig(config_path) != 0) {
            printf("ParseConfig Faild! \r\n");
            return false;
        }
	int ret;
        std::string runlib_path;
        if (ret != configData.GetStringValue("runlib_path", runlib_path)) {
            printf("can't get runlib_path, please check \r\n");
            return false;
        }

	void *handle = dlopen(runlib_path.c_str(), RTLD_LAZY);
        if(handle == NULL) {
            printf("error dlopen - %s \r\n", dlerror());
            return false;
        }

	char * dl_error;
	creator_exec create = (creator_exec)dlsym(handle, "CreateRunTime");
	if((dl_error = dlerror()) != NULL) {
	    printf("find sym error %s \r\n", dl_error);
            return false;
	}

        exec = (*create)();
        ret = exec->Preprocess(config_path, inputTensorVec, outputTensorVec);
        if (ret != 0) {
            printf("Failed to exec Preprocess\r\n");
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

        int ret = exec->Forword(inputTensorVec, outputTensorVec);
        if (ret != 0) {
            printf("Failed to exec Forword \r\n");
            return false;
        }
        return true;
    }

    bool Postrun(Node* node)
    {
        int ret = exec->Postprocess();
        if (ret != 0) {
            printf("Failed to exec Postprocess \r\n");
            return false;
        }
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
