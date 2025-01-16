#pragma once

#include <GU/GU_Detail.h>

#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>

#include <OP/OP_OperatorTable.h>
#include <SOP/SOP_Node.h>

#include <UT/UT_BoundingBox.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Matrix4.h>

#include <DM/DM_RenderTable.h>
#include <DM/DM_Detail.h>

#include "GUI/GUI_ZibraVDBRender.h"

namespace Zibra
{
    class GU_PrimZibraVDB : public GEO_Primitive
    {
    public:
        explicit GU_PrimZibraVDB(GEO_Detail* gdp) noexcept;
        ~GU_PrimZibraVDB() noexcept final;
    public:
        void registerMyself(GA_PrimitiveFactory* factory) noexcept;

    public:
        bool getBBox(UT_BoundingBox* bbox) const noexcept final;
        UT_Vector3 computeNormal() const noexcept final;

        void transform(const UT_Matrix4& mat) noexcept final;

        void copyPrimitive(const GEO_Primitive* src) noexcept final;

        void reverse(void) final;

        int detachPoints(GA_PointGroup& grp) noexcept final;

        bool isDegenerate() const noexcept final;

        fpreal calcVolume(const UT_Vector3& refpt) const noexcept final;
        fpreal calcArea() const noexcept final;

    private:
        UT_BoundingBox m_AABB = {};
        UT_Matrix4 m_XForm = {};
        GEO_Point* m_Point = nullptr;
        GEO_Vertex* m_Vertex = nullptr;

    private:
        static GA_PrimitiveDefinition* ms_PrimDef;
    };
} // namespace Zibra
