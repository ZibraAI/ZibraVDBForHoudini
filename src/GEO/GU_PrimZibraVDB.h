#pragma once

#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GU/GU_Detail.h>
#include <GUI/GUI_PrimitiveHook.h>
#include <OP/OP_OperatorTable.h>
#include <RE/RE_Render.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Matrix4.h>

#include <GR/GR_Detail.h>
#include <GR/GR_RenderHook.h>
#include <GR/GR_RenderTable.h>

#include <DM/DM_RenderTable.h>
#include <DM/DM_Detail.h>



namespace Zibra
{
    class GUI_ZibraVDBRenderHook : public GUI_PrimitiveHook
    {
    public:
        GUI_ZibraVDBRenderHook() = default;
        virtual ~GUI_ZibraVDBRenderHook() = default;

    public:
        void renderWire(GU_Detail* gdp, RE_Render& ren, const GR_AttribOffset& ptinfo, const GR_DisplayOption* dopt, float lod,
                                const GU_PrimGroupClosure* hidden_geometry) final;
        void renderShaded(GU_Detail* gdp, RE_Render& ren, const GR_AttribOffset& ptinfo, const GR_DisplayOption* dopt, float lod,
                                  const GU_PrimGroupClosure* hidden_geometry) final;
        const char *getName() const final { return "GR_RenderOctree"; }
    };

    class GU_PrimZibraVDB : public GEO_Primitive
    {
    public:
        void registerMyself(GA_PrimitiveFactory* factory) noexcept
        {
            // register GU primitive
            theDef = factory->registerDefinition("zibravdb", gu_newPrimAwesome, GA_FAMILY_NONE);
            theDef->setLabel("ZibraVDB");
            registerIntrinsics(*theDef);

            constexpr int priority = 0;
            DM_RenderTable::getTable()->registerGEOHook(new GUI_ZibraVDBRenderHook{}, theDef->getId(), priority, GUI_HOOK_FLAG_NONE);
        }

    public:
        explicit GU_PrimZibraVDB(GEO_Detail* gdp) noexcept;
        ~GU_PrimZibraVDB() final;

    public:

        bool getBBox(UT_BoundingBox* bbox) const final;
        UT_Vector3 computeNormal() const final;

        int save(ostream& os, int binary) const final;
        bool load(UT_IStream& is) final;

        bool evaluatePoint(GEO_Vertex& result, GEO_AttributeHandleList& hlist, fpreal u, fpreal v, uint du, uint dv) const final;

        void transform(const UT_Matrix4& mat) final;

        void copyPrimitive(const GEO_Primitive* src) final;

        void reverse(void) final;

        int detachPoints(GA_PointGroup& grp) final;

        bool isDegenerate() const final;

        fpreal calcVolume(const UT_Vector3& refpt) const final;
        fpreal calcArea() const final;

        // // I CAN'T SEE THEM in GEO_PrimVolume class, but no compilation possible without them.
        // int isPointGroupUsed(const GB_PointGroup& grp) const;
        // int isPointUsed(GB_Element* pt) const;
        // void copyOffsetPrimitive(const GEO_Primitive* src, int basept);
        // void pointDeleted(GB_Element*) ;
        // int pointCanDelete(GB_Element*) const final;

    private:
        UT_BoundingBox m_AABB = {};
        UT_Matrix4 m_XForm = {};
        GEO_Point* m_Point = nullptr;
        GEO_Vertex* m_Vertex = nullptr;

    private:
        static GA_PrimitiveDefinition* theDef;
    };
} // namespace Zibra
