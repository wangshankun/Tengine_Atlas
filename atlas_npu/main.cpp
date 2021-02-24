//#include "atlas800/atlas800.h"
#include "atlas800_extern.h"
#include <algorithm>

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


#include <pthread.h>
#include <stdio.h>
string configName;
struct Run {
    
    shared_ptr<HiInterface> net;
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;
    int deviceId = 0;
    aclrtContext context = nullptr;
    
    int Init(string configName)
    {
        net = shared_ptr<HiInterface>(new HiInterface(configName)); 

        AclInitial(deviceId, context);

        AclRtSetCurrentContext(context);
        
        int ret = net->HiInit(inputTensorVec, outputTensorVec);
        if (ret != 0) {
            printf("Failed to init ");
            return ret;
        }

        //准备输入数据和输出内存
        for(int i = 0; i < inputTensorVec.size(); i++)
        {
            inputTensorVec[i]->data = (uint8_t*)malloc(inputTensorVec[i]->len);
        }
        for(int i = 0; i < outputTensorVec.size(); i++)
        {
            outputTensorVec[i]->data = (uint8_t*)malloc(outputTensorVec[i]->len);
        }

        return 0;
    }
    
    int Forward()
    {
        AclRtSetCurrentContext(context);
        int dynamic_batch_size = 0; 
        net->HiForword(inputTensorVec, outputTensorVec, dynamic_batch_size);
        return 0;
    }
    
    int Destory()
    {  
        AclRtSetCurrentContext(context);
        for(int i = 0; i < inputTensorVec.size(); i++)
        {
            free(inputTensorVec[i]->data); 
        }
        for(int i = 0; i < outputTensorVec.size(); i++)
        {
            free(outputTensorVec[i]->data);
        }

        net->HiDestory();
        AclDestroy(deviceId, context);
        ReleaseAcl();
    }
};

void* exe(void* args)
{
    Run* run = (Run*)args;
    printf();
    run->Forward();
    printf();
    return 0;
}

void* init(void* args)
{
    Run* run = (Run*)args;
    printf();
    run->Init(configName);
    printf();
    return 0;
}

int main(int argc, const char* argv[])
{
    SetLogLevel(5);//off log
    //SetLogLevel(0);//all log on
    std::string config_name(argv[1], argv[1] + strlen(argv[1]));
    configName = config_name;
    std::size_t dir_pos  = configName.find_last_of("/");
    std::string dir_path = configName.substr(0, dir_pos);
    InitAcl(dir_path + "/acl.json");

    Run run;
    pthread_t ntid;
    pthread_create(&ntid, NULL, init, &run);
    sleep(1);
    printf();
    pthread_create(&ntid, NULL, exe, &run);
    printf();
    sleep(1);
    run.Destory();
    return 0;
}
