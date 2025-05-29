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

PXR_NAMESPACE_USING_DIRECTIVE

using namespace Zibra;

class ZibraVDBPluginRegistry::_Impl
{
public:
    _Impl() {}
    ~_Impl() {}
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

    UsdPrimRange range = stage->TraverseAll();
    for (const UsdPrim& prim : range)
    {
        if (prim.GetTypeName() == "ZibraVDBVolume")
        {
            //TODO decompress here
        }
    }
}