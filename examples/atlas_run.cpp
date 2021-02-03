#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include "tengine_c_api.h"
#include <sys/time.h>
#include "common.hpp"

#define DIM_NUM 4

bool get_input_data_from_binfile(std::string &image_file, void *input_data, int input_length)
{
	FILE *fp = fopen(image_file.c_str(), "rb");
	if (fp == nullptr)
	{
		std::cout << "Open input data file failed: " << image_file << "\n";
		return false;
	}

	int res = fread(input_data, 1, input_length, fp);
	if (res != input_length)
	{
		std::cout << "Read input data file failed: " << image_file << "\n";
		return false;
	}
	fclose(fp);
	return true;
}
bool get_float_tensor_data_from_textfile(std::string &file_path, float* input_data, int length)
{
	FILE* fp = fopen(file_path.c_str(),"rb");
	float fval = 0.0;
						          
	printf("\n*****input value(read from tensor file:%s) *****\n", file_path.c_str());
	for (int i=0; i<length; i++){
		if(fscanf( fp, "%f ", &fval ) != 1)
		{   
			printf("Read tensor file fail.\n");
			printf("Please check file lines or if the file contains illegal characters\n");
			return false;
		}   
		input_data[i]=fval;
		if (i < 8)
			printf("input_data[%d]=%f\n",i,fval);

	}   
	printf("*****get input value finished*****\n\n");
	fclose(fp);
	return true;
}

int main(int argc, char *argv[])
{

    const std::string root_path = get_root_path();
    std::string proto_file;
    std::string model_file;
    std::string image_file;
    const char * device=nullptr;
    set_log_level(LOG_DEBUG);
    int res;
    int image_w = 300;
    int image_h= 300;
    // -p mssd.prototxt -m mssd.caffemodel -i img.jpg -w 300
    while( ( res=getopt(argc,argv,"p:m:i:w:h:ud:"))!= -1)
    {
        switch(res)
        {
            case 'p':
                proto_file=optarg;
                break;
            case 'm':
                model_file=optarg;
                break;
            case 'i':
                image_file=optarg;
                break;
			case 'w':
				image_w=atoi(optarg);
				break;
			case 'h':
				image_h=atoi(optarg);
				break;
			case 'd':
				device=optarg;
				break;
            case 'u':
                std::cout << "[Usage]: " << argv[0] << " [-u]\n"
                          << "   [-p proto_file] [-m model_file] \n";
				return 0;
			default:
                break;
        }
    }

    if(proto_file.empty())
    {
        std::cout<< "proto file not specified\n";
        return 1;

    }
    if(model_file.empty())
    {
        std::cout<< "model file not specified\n";
        return 1;
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
    if(!check_file_exist(proto_file) or (!check_file_exist(model_file)))
    {
        return 1;
    }
    // create graph
    graph_t graph = create_graph(nullptr, "caffe", proto_file.c_str(), model_file.c_str());

    if(graph == nullptr)
    {
        std::cout << "Create graph failed\n";
        std::cout << " ,errno: " << get_tengine_errno() << "\n";
        return 1;
    }
    if(device!=nullptr)
    {
        set_graph_device(graph,device);
    }
    
    //printf("========dump graph==========\n");
    //dump_graph(graph);

    // input
    int img_h = image_h;
    int img_w = image_w;
    int img_size = img_h * img_w * 3;
    printf("input image w=%d, h=%d\n", img_w, img_h);
    float *input_data = (float *)malloc(sizeof(float) * img_size);

    int node_idx=0;
    int tensor_idx=0;
    tensor_t input_tensor = get_graph_input_tensor(graph, node_idx, tensor_idx);
    if(input_tensor == nullptr)
    {
        std::printf("Cannot find input tensor,node_idx: %d,tensor_idx: %d\n", node_idx, tensor_idx);
        return -1;
    }

    int dims[] = {1, 3, img_h, img_w};
    set_tensor_shape(input_tensor, dims, 4);
    int ret = prerun_graph(graph);
    if(ret != 0)
    {
        std::cout << "Prerun graph failed, errno: " << get_tengine_errno() << "\n";
     //   return 1;
    }
    printf("============after prerun_graph\n");
    int repeat_count = 1;
    const char *repeat = std::getenv("REPEAT_COUNT");

    if (repeat)
        repeat_count = std::strtoul(repeat, NULL, 10);

	//warm up

    set_tensor_buffer(input_tensor, input_data, img_size * sizeof(float));

    struct timeval t0, t1;
    float total_time = 0.f;
    for (int i = 0; i < repeat_count; i++)
    {
        gettimeofday(&t0, NULL);
        set_tensor_buffer(input_tensor, input_data, img_size * 4);
        ret = run_graph(graph, 1);
        gettimeofday(&t1, NULL);
        float mytime = (float)((t1.tv_sec * 1000000 + t1.tv_usec) - (t0.tv_sec * 1000000 + t0.tv_usec)) / 1000;
        total_time += mytime;

    }
    std::cout << "--------------------------------------\n";
    std::cout << "repeat " << repeat_count << " times, avg time per run-graph is " << total_time / repeat_count << " ms\n";

    int out_idx = 0;
    int out_node_num = get_graph_output_node_number(graph);
    printf("output node num %d\n", out_node_num);
    for (size_t out_node_idx = 0; out_node_idx < out_node_num; ++out_node_idx) {
        auto out_node = get_graph_output_node(graph, out_node_idx);
        auto out_tensor_num = get_node_output_number(out_node);
		//printf("node %d has %d out tensor\n", out_node_idx, out_tensor_num);
        out_idx += out_tensor_num;
    }
    printf("total output num %d\n", out_idx);

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
