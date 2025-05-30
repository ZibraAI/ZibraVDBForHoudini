#ifndef ZIBRAVDB_PLUGIN_REGISTRY_H
#define ZIBRAVDB_PLUGIN_REGISTRY_H

#include <pxr/pxr.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/base/tf/notice.h>
#include <pxr/base/tf/weakBase.h>

#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_Assert.h>

#include <openvdb/openvdb.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra
{
    class ZibraVDBPluginRegistry : public TfWeakBase
    {
    public:
        static ZibraVDBPluginRegistry& GetInstance();
        void Initialize();
    private:
        ZibraVDBPluginRegistry();
        ~ZibraVDBPluginRegistry();

        void OnStageOpened(const UsdNotice::StageContentsChanged& notice);
        void ProcessStage(const UsdStagePtr& stage);
        void ProcessZibraVDBVolume(const UsdStagePtr& stage, const UsdPrim& prim);
        void DecompressAndCreateVDBVolume(const UsdStagePtr& stage, const std::string& filePath, int frameIndex, const SdfPath& primPath);
        std::string WriteTemporaryVDBFile(const openvdb::GridPtrVec& vdbGrids, const std::string& basePath);
        /*void CreateOpenVDBVolume(const UsdStagePtr& stage, const SdfAssetPath& filePath, const SdfPath& volumePath);*/

        class _Impl;
        std::unique_ptr<_Impl> _impl;
        bool _initialized;
        friend class TfSingleton<ZibraVDBPluginRegistry>;
    };
}

#endif // ZIBRAVDB_PLUGIN_REGISTRY_H