#pragma once

#include <DM/DM_Detail.h>
#include <DM/DM_RenderTable.h>
#include <GR/GR_Primitive.h>
#include <GUI/GUI_PrimitiveHook.h>
#include <RE/RE_Render.h>

namespace Zibra
{
    class GUI_ZibraVDBRenderHook : public GUI_PrimitiveHook
    {
    public:
        explicit GUI_ZibraVDBRenderHook() noexcept;
        ~GUI_ZibraVDBRenderHook() noexcept final = default;

    public:
        GR_Primitive* createPrimitive(const GT_PrimitiveHandle& gt_prim, const GEO_Primitive* geo_prim, const GR_RenderInfo* info,
                                      const char* cache_name, GR_PrimAcceptResult& processed) noexcept final;
    };

    class GUI_PrimFramework : public GR_Primitive
    {
    public:
        GUI_PrimFramework(const GR_RenderInfo* info, const char* cache_name, const GEO_Primitive* prim);
        ~GUI_PrimFramework() override;

    public:
        const char* className() const override
        {
            return "GUI_PrimFramework";
        }

        /// See if the primitive can be consumed by this GR_Primitive. Only
        /// primitives from the same detail will ever be passed in. If the
        /// primitive hook specifies GUI_HOOK_COLLECT_PRIMITIVES then it is
        /// possible to have this called more than once for different GR_Primitives.
        /// A GR_Primitive that collects multiple GT or GEO primitives is
        /// responsible for keeping track of them (in a list, table, tree, etc).
        GR_PrimAcceptResult acceptPrimitive(GT_PrimitiveType t, int geo_type, const GT_PrimitiveHandle& ph,
                                            const GEO_Primitive* prim) override;
        /// Called whenever certain geometry-affecting display options are changed.
        /// This gives you the opportunity to see if you need to update the
        /// geometry based on what changed (you will need to cache the display
        /// options that you are interested in to determine this). Return:
        ///    DISPLAY_UNCHANGED  - no update required.
        ///    DISPLAY_CHANGED    - additional geometry attributes may be required
        ///    DISPLAY_VERSION_CHANGED - existing geometry must be redone
        /// If 'first_init' is true, this is being called to initialize any cached
        /// display options. The return value will be ignored. This will happen
        /// just after the primitive is created.
        GR_DispOptChange displayOptionChange(const GR_DisplayOption& opts, bool first_init) override;

        /// If this primitive requires an update when the view changes, return true.
        bool updateOnViewChange(const GR_DisplayOption&) const override
        {
            return false;
        }

        /// If updateOnViewChange() returns true, this is called when the view
        /// changes, but no other update reasons are present. Note that you do not
        /// have access to the primitive in this method, so you will need to cache
        /// anything relating to the primitive required by this method.
        void viewUpdate(RE_RenderContext r, const GR_ViewUpdateParms& parms) override
        {
        }
        /// For primitives that may need updating if the GL state changes, this
        /// hook allows you to perform a check if no update is otherwise required.
        /// Return true to have checkGLState() called. Returning true from that
        /// will trigger an update with the GR_GL_STATE_CHANGED reason set.
        /// @{
        bool needsGLStateCheck(const GR_DisplayOption& opts) const override
        {
            return false;
        }
        bool checkGLState(RE_RenderContext r, const GR_DisplayOption& opts) override
        {
            return false;
        }
        /// @}

        /// Called whenever the parent detail is changed, draw modes are changed,
        /// selection is changed, or certain volatile display options are changed
        /// (such as level of detail).
        void update(RE_RenderContext r, const GT_PrimitiveHandle& primh, const GR_UpdateParms& p) override;
        /// return true if the primitive is in or overlaps the view frustum.
        /// always returning true will effectively disable frustum culling.
        bool inViewFrustum(const UT_Matrix4D& objviewproj, const UT_BoundingBoxD* bbox) override
        {
            return true;
        }
        /// Called to do a variety of render tasks (beauty, wire, shadow, object
        /// pick)
        void render(RE_RenderContext r, GR_RenderMode render_mode, GR_RenderFlags flags, GR_DrawParms draw_parms) override;

        /// Called when decoration 'decor' is required to be rendered.
        void renderDecoration(RE_RenderContext r, GR_Decoration decor, const GR_DecorationParms& p) override;

        /// Render this primitive for picking, where pick_type is defined as one of
        /// the pickable bits in GU_SelectType.h (like GU_PICK_GEOPOINT)
        /// return the number of picks
        int renderPick(RE_RenderContext r, const GR_DisplayOption* opt, unsigned int pick_type, GR_PickStyle pick_style,
                       bool has_pick_map) override;
    };
} // namespace Zibra