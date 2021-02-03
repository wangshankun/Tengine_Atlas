#ifndef __ATLAS_PARAM_HPP__
#define __ATLAS_PARAM_HPP__

#include <vector>

#include "parameter.hpp"

namespace TEngine {

struct AtlasParam : public NamedParam
{
    std::string model_name;
    int total_output_layer_nums;
    std::vector<std::vector<int>> output_shape_v;

    DECLARE_PARSER_STRUCTURE(AtlasParam)
    {
        DECLARE_PARSER_ENTRY(model_name);
        DECLARE_PARSER_ENTRY(total_output_layer_nums);
    };
};

}    // namespace TEngine

#endif

