#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include "tengine_c_api.h"
#include <sys/time.h>
#include <fstream>
#include "common.hpp"
using std::fstream;

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

#define DIM_NUM 4

#define savefile(name, buffer, size) do\
{\
  FILE *out = fopen(name, "wb");\
  if(out != NULL)\
  {\
        fwrite (buffer , sizeof(char), size, out);\
        fclose (out);\
  }\
} while(0)

#define readfile(name, buffer, size) do\
{\
  FILE *out = fopen(name, "rb");\
  if(out != NULL)\
  {\
        fread (buffer , sizeof(char), size, out);\
        fclose (out);\
  }\
} while(0)

int main(int argc, char *argv[])
{
    const std::string root_path = get_root_path();
    set_log_level(LOG_DEBUG);
    int res;
    std::string proto_file;
    std::string input_file;
    while( (res = getopt(argc, argv, "p:i:h:"))!= -1)
    {
        switch(res)
        {
            case 'p':
                proto_file = optarg;
                break;
            case 'i':
                input_file = optarg;
                break;
            case 'h':
                std::cout << "[Usage]: " << argv[0] << " [-h] \n"
                          << " [-p proto_file]  [-i input_file] \n";
                return 0;
        default:
                break;
        }
    }

    // init tengine
    if(init_tengine() < 0)
    {
        std::cout << " init tengine failed\n";
        return 1;
    }
    if(request_tengine_version("0.9") != 1)
    {
        std::cout << " request tengine version failed\n";
        return 1;
    }

    // check file
    if(!check_file_exist(proto_file) or (!check_file_exist(input_file)))
    {
        std::cout << "proto_file or input_file not exist \n";
        return 1;
    }
    std::string empty_model = "empty.caffemodel";
    std::ofstream file(empty_model);
    graph_t graph = create_graph(nullptr, "caffe", proto_file.c_str(), empty_model.c_str());
    remove(empty_model.c_str());

    if(graph == nullptr)
    {
        std::cout << "Create graph failed\n";
        std::cout << " ,errno: " << get_tengine_errno() << "\n";
        return 1;
    }
    
    //printf("========dump graph==========\n");
    //dump_graph(graph);

    int node_idx=0;
    int tensor_idx=0;
    tensor_t input_tensor = get_graph_input_tensor(graph, node_idx, tensor_idx);
    if(input_tensor == nullptr)
    {
        printf("Cannot find input tensor,node_idx: %d,tensor_idx: %d\n", node_idx, tensor_idx);
        return -1;
    }

    printf();
    int ret = prerun_graph(graph);
    if(ret != 0)
    {
        std::cout << "Prerun graph failed, errno: " << get_tengine_errno() << "\n";
     //   return 1;
    }

    int dims[4] = {0};
    get_tensor_shape(input_tensor, dims, 4);
    int img_size = dims[0]*dims[1]*dims[2]*dims[3];
    printf("input image %d,%d,%d,%d\n", dims[0],dims[1],dims[2],dims[3]);
    float *input_data = (float *)malloc(sizeof(float) * img_size);
    readfile(input_file.c_str(), input_data, sizeof(float) * img_size);
    set_tensor_shape(input_tensor, dims, 4);
    set_tensor_buffer(input_tensor, input_data, img_size * sizeof(float));

    double elapsed;
    struct timespec start, finish;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ret = run_graph(graph, 1);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("run_graph elapsed time:%f\r\n",elapsed);

    int out_node_num = get_graph_output_node_number(graph);
    for (size_t out_node_idx = 0; out_node_idx < out_node_num; ++out_node_idx)
    {
        auto out_node = get_graph_output_node(graph, out_node_idx);
        auto out_tensor_num = get_node_output_number(out_node);
        for(size_t out_tensor_idx = 0; out_tensor_idx < out_tensor_num; out_tensor_idx++)
        {
            printf("out_node_idx:%d out_tensor_num:%d \r\n",out_node_idx, out_tensor_idx);
            //第 node_idx 的第 tensor_idx 个tensor
            tensor_t out_tensor = get_graph_output_tensor(graph, out_node_idx, out_tensor_idx);
            //int out_dim[4];
            //get_tensor_shape(out_tensor, out_dim, 4);
            const float* out_data = ( float* )get_tensor_buffer(out_tensor);
            const char*  out_name = get_tensor_name(out_tensor);
            const int    out_size = get_tensor_buffer_size(out_tensor);
        std::string str(out_name);
            replace(str.begin(),str.end(),'/','_');
            printf("%p %d %s\r\n",str.c_str(),out_size,out_name);
            savefile(str.c_str(), out_data, out_size);
        }
    }
    release_graph_tensor(input_tensor);
    ret = postrun_graph(graph);
    if(ret != 0)
    {
        std::cout << "Postrun graph failed, errno: " << get_tengine_errno() << "\n";
        return 1;
    }
    free(input_data);
    destroy_graph(graph);
    release_tengine();

    return 0;
}
