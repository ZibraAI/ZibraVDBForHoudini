#include "GU_PrimZibraVDB.h"

namespace Zibra
{
    GA_PrimitiveDefinition* GU_PrimZibraVDB::theDef = nullptr;

    GU_PrimZibraVDB::GU_PrimZibraVDB(GEO_Detail* gdp) noexcept
        : GEO_Primitive(gdp)
    {
        m_Point = gdp->appendPoint();
        m_Point->setPos(UT_Vector3{0, 0, 0});
        m_Vertex = new GEO_Vertex(gdp, m_Point);

        // TODO: load data
        gdp->appendPrimitive(this);
    }

    GU_PrimZibraVDB::~GU_PrimZibraVDB()
    {
        delete m_Vertex;
        // GEO_Detail* gdp = getParent();
        // gdp->removeUnusedPoints();
    }

    bool GU_PrimZibraVDB::getBBox(UT_BoundingBox* bbox) const
    {
        *bbox = UT_BoundingBox{};
        bbox->setBounds(0, 0, 0, 1, 1, 1);
        return bbox->area() ? true : false;
    }

    UT_Vector3 GU_PrimZibraVDB::computeNormal() const
    {
        return UT_Vector3{0,1,0};
    }

    void GU_PrimZibraVDB::transform(const UT_Matrix4& mat)
    {
        m_XForm = mat;
    }

    void GU_PrimZibraVDB::copyPrimitive(const GEO_Primitive* src)
    {
        const auto* zibravdbPrim = dynamic_cast<const GU_PrimZibraVDB*>(src);
        if(!zibravdbPrim) return;
        m_AABB = zibravdbPrim->m_AABB;
        m_XForm = zibravdbPrim->m_XForm;

        //TODO: finish

        GEO_Primitive::copyPrimitive(src);
    }

    void GU_PrimZibraVDB::reverse()
    {
        // NOOP. We have just 1 point.
    }

    int GU_PrimZibraVDB::detachPoints(GA_PointGroup& grp)
    {
        return (grp.contains(m_Point) ? -2 : 0);
    }

    bool GU_PrimZibraVDB::isDegenerate() const
    {
        return false;
    }

    fpreal GU_PrimZibraVDB::calcVolume(const UT_Vector3& refpt) const
    {
        return m_AABB.volume();
    }

    fpreal GU_PrimZibraVDB::calcArea() const
    {
        return 0.0f;
    }
} // namespace Zibra