#ifndef ATLAS800_EXTERN_H
#define ATLAS800_EXTERN_H

#include <map>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <string.h>
#include <memory>

using namespace std;

struct  ModelProcess;

typedef void *aclrtContext;

enum HiLogLevels {
    HI_LOG_LEVEL_DEBUG = 0,
    HI_LOG_LEVEL_INFO = 1,
    HI_LOG_LEVEL_WARN = 2,
    HI_LOG_LEVEL_ERROR = 3,
    HI_LOG_LEVEL_FATAL = 4,
    HI_LOG_LEVEL_NONE
};

//from acl/acl_base.h aclDataType 
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

// 给host用户使用的tensor
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


void SetLogLevel(uint32_t level);
int  ChangeToDstDir(std::string path);
int  InitAcl(std::string acl_cfg);
int  ReleaseAcl();

class HiInterface {
public:
    HiInterface(std::string configPath);
    ~HiInterface() {};
    int HiDestory();
    int HiInit(std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                                    std::vector<std::shared_ptr<HiTensor>> &outputVec);
    int HiForword(std::vector<std::shared_ptr<HiTensor>> &inputVec,
                                    std::vector<std::shared_ptr<HiTensor>> &outputVec, size_t dynamicBatchSize = 0);
    int SoftmaxTopkInit(std::shared_ptr<HiTensor>& input_tensor,
                              std::shared_ptr<HiTensor>& output_tensor,
                              int top_k,  int cpu_thread_num);
    int SoftmaxTopkExec(std::shared_ptr<HiTensor>& input_tensor,
                              std::shared_ptr<HiTensor>& output_tensor);
private:
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
