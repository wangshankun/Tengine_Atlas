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

#include <mutex>

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
    
std::mutex  g_mutex;
struct AtlasOps : public NodeOps
{
    HiInterface* net;
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;

    
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
        g_mutex.lock();
        net = new HiInterface(config_name); 
        int ret = net->HiInit(inputTensorVec, outputTensorVec);
        g_mutex.unlock();
        if (ret != 0) {
            printf("Failed to init \r\n");
            return false;
        }
        
        return true;
    }

    bool Run(Node * node)
    {
        g_mutex.lock();
        for(int i = 0; i < inputTensorVec.size(); i++)
        {
            inputTensorVec[i]->data = (uint8_t*)malloc(inputTensorVec[i]->len);
        }
        for(int i = 0; i < outputTensorVec.size(); i++)
        {
            outputTensorVec[i]->data = (uint8_t*)malloc(outputTensorVec[i]->len);
        }
        
        net->HiForword(inputTensorVec, outputTensorVec, 0);
        g_mutex.unlock();
        return true;
    }

    bool Postrun(Node* node)
    {   //释放内存，销毁npu资源
        for(int i = 0; i < inputTensorVec.size(); i++)
        {
            free(inputTensorVec[i]->data); 
        }
        for(int i = 0; i < outputTensorVec.size(); i++)
        {
            free(outputTensorVec[i]->data);
        }

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
