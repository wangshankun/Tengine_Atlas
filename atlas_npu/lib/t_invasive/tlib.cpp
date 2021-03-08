#include "atlas800.h"
#include "tlib.h"

class NpuDevice//ACL只能初始化一次
{
public:
    // 获取单实例对象
    static NpuDevice &GetInstance();
private:
    // 禁止外部构造/析构
    NpuDevice();
    ~NpuDevice();
    // 禁止外部复制构造
    NpuDevice(const NpuDevice &signal);
    // 禁止外部赋值操作
    const NpuDevice &operator=(const NpuDevice &signal);
};

NpuDevice &NpuDevice::GetInstance()
{
    //局部静态特性的方式实现单实例
    static NpuDevice signal;
    return signal;
}

NpuDevice::NpuDevice()
{
    std::string empty_acl_conf = "acl.json";
    std::ofstream file(empty_acl_conf);
    InitAcl(empty_acl_conf);
    remove(empty_acl_conf.c_str());
}

NpuDevice::~NpuDevice()
{
    ReleaseAcl();
}

int OpRunTime::Preprocess(string configName, 
                    std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                    std::vector<std::shared_ptr<HiTensor>> &outputVec)
{
    net_ = std::shared_ptr<HiInterface>(new HiInterface(configName));  
    return net_->HiInit(inputVec, outputVec);
}

int OpRunTime::Forword(std::vector<std::shared_ptr<HiTensor>> &inputVec, 
                    std::vector<std::shared_ptr<HiTensor>> &outputVec)
{
    return net_->HiForword(inputVec, outputVec);
}

int OpRunTime::Postprocess()
{
    if(net_ != nullptr)
    {
        return net_->HiDestory();
    }
    return 0;
}

RunTime* CreateRunTime()
{
    NpuDevice::GetInstance();
    return new OpRunTime();
}
