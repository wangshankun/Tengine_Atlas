#ifndef __OP_RUNTIME_H__
#define __OP_RUNTIME_H__

#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <fstream>

using namespace std;

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
