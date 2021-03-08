#ifndef __OP_RUNTIME_H__
#define __OP_RUNTIME_H__

#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

using namespace std;

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

#endif
