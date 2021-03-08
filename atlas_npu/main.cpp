#include <functional>
#include <stdlib.h>
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
#include <assert.h>
#include <dlfcn.h>
#include <vector>
#include <memory>

#include "tlib.h"

#define readfile(name, buffer, size) do\
{\
  FILE *out = fopen(name, "rb");\
  if(out != NULL)\
  {\
        fread (buffer , sizeof(char), size, out);\
        fclose (out);\
  }\
} while(0)

#define savefile(name, buffer, size) do\
{\
  FILE *out = fopen(name, "wb");\
  if(out != NULL)\
  {\
        fwrite (buffer , sizeof(char), size, out);\
        fclose (out);\
  }\
} while(0)

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

typedef RunTime* (*creator_exec)();

int main(int argc, const char* argv[])
{
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;
    string config_path = "/root/Tengine_Atlas/cnn_ctc_sx/setup.config";
    string runlib_path = "/root/Tengine_Atlas/atlas_npu/lib/dist/libt_invasive.so";
    printf();
    void *handle = dlopen(runlib_path.c_str(), RTLD_LAZY);
        if(handle == NULL) {
            printf("error dlopen - %s \r\n", dlerror());
            return false;
        }
    printf();
    char * dl_error;
    creator_exec create = (creator_exec)dlsym(handle, "CreateRunTime");
    if((dl_error = dlerror()) != NULL) {
        printf("find sym error %s \r\n", dl_error);
            return false;
    }
    printf();
    RunTime *exec = (*create)();
    int ret = exec->Preprocess(config_path, inputTensorVec, outputTensorVec);
    printf();
    if (ret != 0) {
        printf("Failed to exec Preprocess\r\n");
        return false;
    }
    printf();
    return 0;
}
