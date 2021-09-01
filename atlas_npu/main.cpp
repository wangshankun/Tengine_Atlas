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

#include <map>
#include <sstream>
#include <string>

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

// Remove spaces from both left and right based on the string
inline void Trim(string &str)
{
    str.erase(str.begin(), find_if(str.begin(), str.end(), not1(ptr_fun(::isspace))));
    str.erase(find_if(str.rbegin(), str.rend(), not1(ptr_fun(::isspace))).base(), str.end());
    return;
}

inline int ParseConfig(const string &fileName, map<string, string> &configData)
{
    // Open the input file
    ifstream inFile(fileName);
    if (!inFile.is_open()) {
        cout << "cannot read setup.config file!" << endl;
        return -1;
    }
    string line, newLine;
    int startPos, endPos, pos;
    // Cycle all the line
    while (getline(inFile, line)) {
        if (line.empty()) {
            continue;
        }
        startPos = 0;
        endPos = line.size() - 1;
        pos = line.find("#"); // Find the position of comment
        if (pos != -1) {
            if (pos == 0) {
                continue;
            }
            endPos = pos - 1;
        }
        newLine = line.substr(startPos, (endPos - startPos) + 1); // delete comment
        pos = newLine.find('=');
        if (pos == -1) {
            continue;
        }
        string na = newLine.substr(0, pos);
        Trim(na); // Delete the space of the key name
        string value = newLine.substr(pos + 1, endPos + 1 - (pos + 1));
        Trim(value);                                   // Delete the space of value
        configData.insert(make_pair(na, value)); // Insert the key-value pairs into configData
    }
    return 0;
}

string GetRunLibPath(string configName)
{
    map<string, string> configData;
    if (ParseConfig(configName, configData) != 0) {
        printf("ParseConfig Faild! \r\n");
        return "";
    }
    string runlib_path;

    if (configData.count("runlib_path") == 0) {
        return "";
    }
    else {
        return configData.find("runlib_path")->second;
    }
}

typedef RunTime* (*creator_exec)();

int main(int argc, const char* argv[])
{
    vector<shared_ptr<HiTensor>>  inputTensorVec;
    vector<shared_ptr<HiTensor>>  outputTensorVec;
    string config_path = "/root/Tengine_Atlas/cnn_ctc_sx/setup.config";
    string runlib_path = GetRunLibPath(config_path);
    if (runlib_path.empty()) return -1;
    void *handle = dlopen(runlib_path.c_str(), RTLD_LAZY);
        if(handle == NULL) {
            printf("error dlopen - %s \r\n", dlerror());
            return -1;
        }
    char * dl_error;
    creator_exec create = (creator_exec)dlsym(handle, "CreateRunTime");
    if((dl_error = dlerror()) != NULL) {
        printf("find sym error %s \r\n", dl_error);
            return -1;
    }
    RunTime *exec = (*create)();
    int ret = exec->Preprocess(config_path, inputTensorVec, outputTensorVec);
    if (ret != 0) {
        printf("Failed to exec Preprocess\r\n");
        return -1;
    }
    return 0;
}
