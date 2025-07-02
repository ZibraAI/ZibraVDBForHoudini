#pragma once
#include <LOP/LOP_Node.h>
#include "Globals.h"

namespace Zibra::ZibraVDBCompressionMarker
{
    constexpr const char* LOP_NODE_NAME = "labs::zibravdb_compression::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* LOP_NODE_LABEL = "Labs ZibraVDB Compression";

    class LOP_ZibraVDBCompressionMarker final : public LOP_Node
    {
    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

        LOP_ZibraVDBCompressionMarker(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~LOP_ZibraVDBCompressionMarker() noexcept final = default;

        OP_ERROR cookMyLop(OP_Context& context) final;
        bool updateParmsFlags() override;
        
        std::string getOutputDirectory(fpreal t) const;
        std::string getOutputFilename(fpreal t) const;
        float getCompressionQuality(fpreal t) const;
        bool getRemoveOriginalFiles(fpreal t) const;

    private:
        std::string getOutputDirectoryRaw(fpreal t) const;
        bool isNodeUpstream(OP_Node* node) const;
    };

    class LOP_ZibraVDBCompressionMarker_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit LOP_ZibraVDBCompressionMarker_Operator() noexcept
            : OP_Operator(LOP_NODE_NAME, LOP_NODE_LABEL, LOP_ZibraVDBCompressionMarker::Constructor,
                          LOP_ZibraVDBCompressionMarker::GetTemplateList(), 1, 1, 0, OP_FLAG_GENERATOR, 0, 1)
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }
    };

} // namespace Zibra::ZibraVDBCompressionMarker