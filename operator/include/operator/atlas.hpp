#ifndef __ATLAS_HPP__
#define __ATLAS_HPP__

#include "atlas_param.hpp"
#include "operator.hpp"

namespace TEngine {

class AtlasNode : public OperatorWithParam<AtlasNode, AtlasParam>
{
public:
    AtlasNode() { name_ = "Atlas";}
    AtlasNode(const AtlasNode&) = default;

    void SetSchema(void) override;

    bool InferShape(const std::vector<TEngine::TShape>&, std::vector<TEngine::TShape>&, int layout) override;
    ~AtlasNode() {}
};

}    // namespace TEngine

#endif
