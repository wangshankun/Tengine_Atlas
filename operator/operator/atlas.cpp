#include "operator/atlas.hpp"
#include "static_graph.hpp"


namespace TEngine {
bool AtlasNode::InferShape(const std::vector<TEngine::TShape>& ishape, std::vector<TEngine::TShape>& oshape, int layout)
{
#ifdef CONFIG_INFO_DBUG
    printf("[Atlas::InferShape]: oshape.size()=%d, total_coutput_layer_nums=%d\n", oshape.size());
#endif
    for (size_t i = 0; i < oshape.size(); ++i)
    {
        TShape shape;
        std::vector<int>& dim=param_.output_shape_v[i];
        shape.SetDataLayout(TENGINE_LAYOUT_NCHW);
        shape.SetDim(dim);
        oshape[i]=shape;
    }
    
    return true;    

}

void AtlasNode::SetSchema(void)
{
}


} //namespace TEngine

