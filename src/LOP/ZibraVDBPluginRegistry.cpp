#include "ZibraVDBPluginRegistry.h"

#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <iostream>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdVol/volume.h>
#include <pxr/usd/usdVol/openVDBAsset.h>

#include "USD/zibraVDBVolume.h"
#include "../SOP/DecompressorManager/DecompressorManager.h"
#include "../bridge/LibraryUtils.h"
#include "../utils/VDBCacheManager.h"

#include <openvdb/io/File.h>
#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace Zibra;

class ZibraVDBPluginRegistry::_Impl
{
public:
    _Impl() {}
    ~_Impl() 
    {
        Utils::VDBCacheManager::GetInstance().CleanupAllTempFiles();
        
        if (LibraryUtils::IsLibraryLoaded())
        {
            m_DecompressorManager.Release();
        }
    }
    
    Helpers::DecompressorManager m_DecompressorManager;
};

ZibraVDBPluginRegistry& ZibraVDBPluginRegistry::GetInstance()
{
    static ZibraVDBPluginRegistry instance;
    return instance;
}

ZibraVDBPluginRegistry::ZibraVDBPluginRegistry()
    : _impl(new _Impl()), _initialized(false)
{
}

ZibraVDBPluginRegistry::~ZibraVDBPluginRegistry()
{
}

void ZibraVDBPluginRegistry::Initialize()
{
    if (_initialized)
        return;

    // TODO do we need this
    auto& registry = PlugRegistry::GetInstance();
    std::string pathToPLugin;
    registry.RegisterPlugins(pathToPLugin);
    TfNotice::Register(TfCreateWeakPtr(this), &ZibraVDBPluginRegistry::OnStageOpened);

    _initialized = true;
}

//static std::unordered_set<std::string> initializedStages;
void ZibraVDBPluginRegistry::OnStageOpened(const UsdNotice::StageContentsChanged& notice)
{
    UsdStageWeakPtr weakStage = notice.GetStage();
    if (!weakStage)
        return;
    ProcessStage(weakStage);
}

/*void addTestCube(const UsdStageRefPtr& stage, const SdfPath& parentPath)
{
    try {
        if (!stage) {
            std::cerr << "Stage is null\n";
            return;
        }

//         const SdfPath cubePath("/TestCube");
        const SdfPath cubePath = parentPath.AppendChild(TfToken("TestCube"));

        // Define the cube prim as a mesh
        UsdPrim cubePrim = stage->DefinePrim(cubePath, TfToken("Mesh"));
        if (!cubePrim.IsValid()) {
            std::cerr << "Failed to define prim at /TestCube\n";
            return;
        }

        UsdGeomMesh cubeMesh(cubePrim);

        // Set points (8 vertices of a unit cube)
        VtArray<GfVec3f> points = {
            { -1, -1, -1 }, { 1, -1, -1 },
            { 1,  1, -1 }, { -1,  1, -1 },
            { -1, -1,  1 }, { 1, -1,  1 },
            { 1,  1,  1 }, { -1,  1,  1 }
        };
        cubeMesh.CreatePointsAttr().Set(points);

        // Define face vertex indices (12 triangles = 6 quad faces)
        VtArray<int> faceVertexIndices = {
            0, 1, 2, 3,  // bottom
            4, 5, 6, 7,  // top
            0, 1, 5, 4,  // front
            2, 3, 7, 6,  // back
            0, 3, 7, 4,  // left
            1, 2, 6, 5   // right
        };
        cubeMesh.CreateFaceVertexIndicesAttr().Set(faceVertexIndices);

        // Each face has 4 vertices
        VtArray<int> faceVertexCounts(6, 4);
        cubeMesh.CreateFaceVertexCountsAttr().Set(faceVertexCounts);

        // Optional: add display color
        VtArray<GfVec3f> colors = { GfVec3f(0.8f, 0.2f, 0.2f) }; // red
        UsdGeomPrimvar colorPrimvar = cubeMesh.CreateDisplayColorPrimvar(UsdGeomTokens->constant);
        colorPrimvar.Set(colors);

        // Add a transform if not already present
        UsdGeomXformable xformable(cubePrim);
        UsdGeomXformOp xform = xformable.AddTransformOp();
        xform.Set(GfMatrix4d(1.0)); // Identity

        std::cout << "addTestCube: Test cube successfully added to stage.\n";

    } catch (const std::exception& e) {
        std::cerr << "addTestCube: Exception occurred - " << e.what() << "\n";
    }
}*/

void ZibraVDBPluginRegistry::ProcessStage(const UsdStagePtr& stage)
{
    if (!stage)
        return;

    openvdb::initialize();
    LibraryUtils::LoadLibrary();
    if (!LibraryUtils::IsLibraryLoaded())
    {
        std::cerr << "ZibraVDB: Compression engine not loaded, skipping ZibraVDBVolume processing\n";
        return;
    }

    UsdPrimRange range = stage->TraverseAll();
    for (const UsdPrim& prim : range)
    {
        if (prim.GetTypeName() == "ZibraVDBVolume")
        {
            ProcessZibraVDBVolume(stage, prim);
        }
    }
}

void ZibraVDBPluginRegistry::ProcessZibraVDBVolume(const UsdStagePtr& stage, const UsdPrim& prim)
{
    ZibraVDBZibraVDBVolume zibraVolume(prim);
//    if (!zibraVolume)
//        return;

    UsdAttribute filePathAttr = zibraVolume.GetFilePathAttr();
    if (!filePathAttr.HasValue())
        return;

    SdfAssetPath assetPath;
    filePathAttr.Get(&assetPath);
    std::string filePath = assetPath.GetResolvedPath();
    if (filePath.empty())
        filePath = assetPath.GetAssetPath();

    if (filePath.empty() || !std::filesystem::exists(filePath))
    {
        std::cerr << "ZibraVDB: File not found: " << filePath << std::endl;
        return;
    }

    int frameIndex = 1;
    VtValue frameIndexValue = prim.GetCustomDataByKey(TfToken("frameIndex"));
    if (!frameIndexValue.IsEmpty() && frameIndexValue.IsHolding<int>())
    {
        frameIndex = frameIndexValue.UncheckedGet<int>();
    }

    try
    {
        DecompressAndCreateVDBVolume(stage, filePath, frameIndex, prim.GetPath());
    }
    catch (const std::exception& e)
    {
        std::cerr << "ZibraVDB: Failed to decompress " << filePath << ": " << e.what() << std::endl;
    }
}

void ZibraVDBPluginRegistry::DecompressAndCreateVDBVolume(const UsdStagePtr& stage, const std::string& filePath, int frameIndex, const SdfPath& primPath)
{
    _impl->m_DecompressorManager.Initialize();

    auto status = _impl->m_DecompressorManager.RegisterDecompressor(UT_String(filePath.c_str()));
    if (status != CE::ZCE_SUCCESS)
    {
        std::cerr << "ZibraVDB: Failed to register decompressor for " << filePath << std::endl;
        return;
    }

    auto frameRange = _impl->m_DecompressorManager.GetFrameRange();
    if (frameIndex < frameRange.start || frameIndex > frameRange.end)
    {
        std::cerr << "ZibraVDB: Frame index " << frameIndex << " out of range [" << frameRange.start << ", " << frameRange.end << "]" << std::endl;
        return;
    }

    auto frameContainer = _impl->m_DecompressorManager.FetchFrame(frameIndex);
    if (!frameContainer || frameContainer->GetInfo().spatialBlockCount == 0)
    {
        std::cerr << "ZibraVDB: Failed to fetch frame " << frameIndex << std::endl;
        if (frameContainer)
            frameContainer->Release();
        return;
    }

    auto gridShuffle = _impl->m_DecompressorManager.DeserializeGridShuffleInfo(frameContainer);
    openvdb::GridPtrVec vdbGrids = {};
    status = _impl->m_DecompressorManager.DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
    _impl->m_DecompressorManager.ReleaseGridShuffleInfo(gridShuffle);

    if (status == CE::ZCE_SUCCESS && !vdbGrids.empty())
    {
        // Use the unified VDB cache manager instead of local file writing
        std::string tempVDBPath = Utils::VDBCacheManager::GetInstance().GetOrCreateVDBCache(filePath, frameIndex, vdbGrids);
        if (tempVDBPath.empty())
        {
            std::cerr << "ZibraVDB: Failed to create VDB cache file" << std::endl;
        }
    }
    else
    {
        std::cerr << "ZibraVDB: Decompression failed or no grids found" << std::endl;
    }

    frameContainer->Release();
}