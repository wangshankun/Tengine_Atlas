#include "tbase.h"
class OpRunTime : public RunTime
{
private:
    std::shared_ptr<HiInterface> net_ = nullptr;
public:
    virtual int Preprocess(string configName,
                            vector<shared_ptr<HiTensor>> &inputVec,
                            vector<shared_ptr<HiTensor>> &outputVec);

    virtual int Forword(vector<shared_ptr<HiTensor>> &inputVec,
                        vector<shared_ptr<HiTensor>> &outputVec);

    virtual int Postprocess();
};

extern "C"  RunTime* CreateRunTime();

