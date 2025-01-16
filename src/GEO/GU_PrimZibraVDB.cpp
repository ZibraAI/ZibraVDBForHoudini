#include "GU_PrimZibraVDB.h"

namespace Zibra
{
    GA_PrimitiveDefinition* GU_PrimZibraVDB::ms_PrimDef = nullptr;

    GU_PrimZibraVDB::GU_PrimZibraVDB(GEO_Detail* gdp) noexcept
        : GEO_Primitive(gdp)
    {
        m_Point = gdp->appendPoint();
        m_Point->setPos(UT_Vector3{0, 0, 0});
        m_Vertex = new GEO_Vertex(gdp, m_Point);

        // TODO: load data
        gdp->appendPrimitive(this->getTypeId());
    }

    GU_PrimZibraVDB::~GU_PrimZibraVDB() noexcept
    {
        delete m_Vertex;
        // GEO_Detail* gdp = getParent();
        // gdp->removeUnusedPoints();
    }

    void GU_PrimZibraVDB::registerMyself(GA_PrimitiveFactory* factory) noexcept
    {
        // register GU primitive
        ms_PrimDef = factory->registerDefinition("zibravdb", gu_newPrimAwesome, GA_FAMILY_NONE);
        ms_PrimDef->setLabel("ZibraVDB");
        registerIntrinsics(*ms_PrimDef);

        constexpr int priority = 0;
        DM_RenderTable::getTable()->registerGEOHook(new GUI_ZibraVDBRenderHook{}, ms_PrimDef->getId(), priority, GUI_HOOK_FLAG_NONE);
    }

    bool GU_PrimZibraVDB::getBBox(UT_BoundingBox* bbox) const noexcept
    {
        *bbox = UT_BoundingBox{};
        bbox->setBounds(0, 0, 0, 1, 1, 1);
        return bbox->area() ? true : false;
    }

    UT_Vector3 GU_PrimZibraVDB::computeNormal() const noexcept
    {
        return UT_Vector3{0,1,0};
    }

    void GU_PrimZibraVDB::transform(const UT_Matrix4& mat) noexcept
    {
        m_XForm = mat;
    }

    void GU_PrimZibraVDB::copyPrimitive(const GEO_Primitive* src) noexcept
    {
        const auto* zibravdbPrim = dynamic_cast<const GU_PrimZibraVDB*>(src);
        if(!zibravdbPrim) return;
        m_AABB = zibravdbPrim->m_AABB;
        m_XForm = zibravdbPrim->m_XForm;

        //TODO: finish

        GEO_Primitive::copyPrimitive(src);
    }

    void GU_PrimZibraVDB::reverse() noexcept
    {
        // NOOP. We have just 1 point.
    }

    int GU_PrimZibraVDB::detachPoints(GA_PointGroup& grp) noexcept
    {
        return (grp.contains(m_Point) ? -2 : 0);
    }

    bool GU_PrimZibraVDB::isDegenerate() const
    {
        return false;
    }

    fpreal GU_PrimZibraVDB::calcVolume(const UT_Vector3& refpt) const noexcept
    {
        return m_AABB.volume();
    }

    fpreal GU_PrimZibraVDB::calcArea() const noexcept
    {
        return 0.0f;
    }
} // namespace Zibra