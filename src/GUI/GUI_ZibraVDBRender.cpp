#include "GUI_ZibraVDBRender.h"

#include <DM/DM_RenderTable.h>
#include <GEO/GEO_PrimPolySoup.h>
#include <GR/GR_DisplayOption.h>
#include <GR/GR_OptionTable.h>
#include <GR/GR_UserOption.h>
#include <GT/GT_GEOPrimitive.h>

namespace Zibra
{

    GUI_ZibraVDBRenderHook::GUI_ZibraVDBRenderHook() noexcept
        : GUI_PrimitiveHook{"ZibraVDB Render Hook", GUI_RENDER_MASK_VULKAN}
    {
    }
    GR_Primitive* GUI_ZibraVDBRenderHook::createPrimitive(const GT_PrimitiveHandle& gt_prim, const GEO_Primitive* geo_prim,
                                                          const GR_RenderInfo* info, const char* cache_name, GR_PrimAcceptResult& processed)
    {
        if (geo_prim->getTypeId().get() == GA_PRIMNONE)
        {
            // We're going to process this prim and prevent any more lower-priority
            // hooks from hooking on it. Alternatively, GR_PROCESSED_NON_EXCLUSIVE
            // could be passed back, in which case any hooks of lower priority would
            // also be called.
            processed = GR_PROCESSED;

            // In this case, we aren't doing anything special, like checking attribs
            // to see if this is a flagged native primitive we want to hook on.
            return new GUI_PrimFramework(info, cache_name, geo_prim);
        }
        return nullptr;
    }


    GUI_PrimFramework::GUI_PrimFramework(const GR_RenderInfo* info, const char* cache_name, const GEO_Primitive* prim)
        : GR_Primitive(info, cache_name, GA_PrimCompat::TypeMask(0))
    {
    }

    GUI_PrimFramework::~GUI_PrimFramework()
    {
    }

    GR_PrimAcceptResult GUI_PrimFramework::acceptPrimitive(GT_PrimitiveType gt_type, int geo_type, const GT_PrimitiveHandle& ph,
                                                           const GEO_Primitive* prim)
    {
        if (geo_type == GA_PRIMNONE)
            return GR_PROCESSED;
        return GR_NOT_PROCESSED;
    }

    GR_Primitive::GR_DispOptChange GUI_PrimFramework::displayOptionChange(const GR_DisplayOption& opts, bool first_init)
    {
        // Check opts to see if any options of interest have changed. Generally
        // these need to be cached in the primitive so that it's possible to tell
        // which have changed. Not all display options will cause a
        // displayOptionChange() check. Non-geometry options such as
        // Scene Antialiasing, Background Image or Grid settings will not cause a
        // check.
        // return value:
        //
        // DISPLAY_UNCHANGED: none of the options the primitive cares about changed,
        //                    so do not update.
        //
        // DISPLAY_CHANGED:   at least one option changed and prim needs an update,
        //                    prompt an update with GR_DISPLAY_OPTIONS_CHANGED
        //
        // DISPLAY_CHANGED_VERSION: an option changed and the prim needs an update
        //                          call update() with GR_DISPLAY_OPTIONS_CHANGED
        //                          and the p.geo_version bumped to update buffers.
        //
        // If 'first_init' is true, this is just being called to cache any options.
        // The return value is ignored in this case, so any checks can be bypassed.

        return DISPLAY_UNCHANGED;
    }
    void GUI_PrimFramework::update(RE_RenderContext r, const GT_PrimitiveHandle& primh, const GR_UpdateParms& p)
    {
        // Fetch the GEO primitive from the GT primitive handle
        const GT_GEOPrimitive* prim = static_cast<const GT_GEOPrimitive*>(primh.get());
        (void)prim; // Silence a compiler warning.

        RV_Render* vkRender =  r.vkRender();
        vkRender->device();
        vkRender->instance();
        // vkRender->beginFrame();

        // Process the reason that this update occurred. The reason is bitfield,
        // and multiple reasons can be sent in one update.

        if (p.reason & (GR_GEO_CHANGED | GR_GEO_ATTRIB_LIST_CHANGED | GR_GEO_TOPOLOGY_CHANGED))
        {
            // geometry itself changed. GR_GEO_TOPOLOGY changed indicates a large
            // change, such as changes in the point, primitive or vertex counts
            // GR_GEO_CHANGED indicates that some attribute data may have changed.
            // GR_GEO_ATTRIB_LIST_CHANGED means that attributes were added,
            // renamed, changed class (eg. vtx->pnt), or deleted.
        }
        if (p.reason & GR_GEO_SELECTION_CHANGED)
        {
            // selection on geometry changed (either temporary or cook selection)
        }

        if (p.reason & (GR_INSTANCE_PARMS_CHANGED | GR_INSTANCE_SELECTION_CHANGED))
        {
            // Object-level instancing has changed, or the instance selection
            // has changed.
        }
        if (p.reason & GR_OBJECT_MODE_CHANGED)
        {
            // the object context changed (object may no longer be selected, is
            // now viewed at the SOP level, or is now templated instead of
            // displayed).
        }
        if (p.reason & GR_DISPLAY_OPTIONS_CHANGED)
        {
            // the draw mode changed, or certain display options that affect
            // geometry changed (eg. decorations, polygon convexing)
        }
        if (p.reason & GR_LOD_CHANGED)
        {
            // The Level of Detail display option changed (GR_DisplayOption::LOD())
        }

        if (p.reason & GR_GL_STATE_CHANGED)
        {
            // The GL state changed. Must override needsGLStateCheck() to return
            // true in order to enable GL state checks. If checkGLState() returns
            // true, this this reason will be sent. Both return false by default.
        }
        if (p.reason & GR_GL_VIEW_CHANGED)
        {
            // The view changed. Must override updateOnViewChange() to return true
            // in order for this reason to be sent.
        }
        if (p.reason & GR_MATERIALS_CHANGED)
        {
            // Parameter(s) on the material(s) attached to this geometry have
            // changed, but the material assignments themselves are the same.
        }

        if (p.reason & GR_MATERIAL_ASSIGNMENT_CHANGED)
        {
            // Materials have been reassigned. Either a material has been added,
            // switched or removed, or some primitives have different materials.
        }
    }
    void GUI_PrimFramework::renderDecoration(RE_RenderContext r, GR_Decoration decor, const GR_DecorationParms& p)
    {
        if (decor >= GR_USER_DECORATION)
        {
            int index = decor - GR_USER_DECORATION;
            const GR_UserOption* user = GRgetOptionTable()->getOption(index);
            // for user-installed options, the option name defines the type.
            if (!strcmp(user->getName(), "unique_option_name"))
            {
                // Render the decoration
            }
        }
    }
    void GUI_PrimFramework::render(RE_RenderContext r, GR_RenderMode render_mode, GR_RenderFlags flags, GR_DrawParms draw_parms)
    {
        std::cerr << "Render " << std::endl;
        // draw_parms.opts:           display options ptr
        // draw_parms.materials:      material atlas, containing all materials. The
        //                            MatID primitive attribute references these by
        //                            id.
        // draw_parms.default_mat_id: material to use when MatID = -1
        // draw_parms.draw_instanced: instanced drawing requested
        // draw_parms.instance_group: draw instanced with 'instance_group' index
        // There are a variety of rendering modes that a primitive can be asked
        // to do. You do not need to support them all. If a render mode is not
        // supported, return immediately.

        switch (render_mode)
        {
        case GR_RENDER_BEAUTY:
            // render out the geometry normally as shaded, with possible options:

            if (flags & GR_RENDER_FLAG_UNLIT)
            {
                // render with no lighting
            }

            if (flags & GR_RENDER_FLAG_WIRE_OVER)
            {
                // requires a wireframe over the shaded geometry
            }
            if (flags & GR_RENDER_FLAG_FLAT_SHADED)
            {
                // no normal interpolation across polygons. Only has meaning for
                // polygon primitive types, generally.
            }
            if (flags & GR_RENDER_FLAG_FADED)
            {
                // do not render textures or geometry attribute colors (Cd)
            }
            break;
        case GR_RENDER_MATERIAL:
            // render out geometry's material properties

            if (flags & GR_RENDER_FLAG_WIRE_OVER)
            {
                // requires a wireframe over the shaded geometry.
            }
            break;
        case GR_RENDER_CONSTANT:
            // render out geometry using a flat, constant color. This color is
            // in r->getUniform(RE_UNIFORM_CONST_COLOR).
            if (flags & GR_RENDER_FLAG_WIRE_OVER)
            {
                // requires a wireframe over the const geometry.
            }
            break;
        case GR_RENDER_WIREFRAME:
            // render wirerame representation.

            if (flags & GR_RENDER_FLAG_FADED)
            {
                // do not render attribute colors on the wireframe (point, prim Cd)
            }
            break;
        case GR_RENDER_GHOST_LINE:
        case GR_RENDER_HIDDEN_LINE:
            // render hidden line representation. Background color is in
            // r->getUniform(RE_UNIFORM_CONST_COLOR), which is the only different
            // between ghost and hidden.
            if (flags & GR_RENDER_FLAG_FADED)
            {
                // do not render attribute colors on the wireframe (point, prim Cd)
            }
            break;
        case GR_RENDER_MATERIAL_WIREFRAME:

            if (flags & GR_RENDER_FLAG_FADED)
            {
                // do not render attribute colors on the wireframe (point, prim Cd)
            }
            break;
        case GR_RENDER_DEPTH:
            // only render solid geometry to the depth buffer. Can use the beauty
            // pass render method if there is no specific shader for this.
            break;
        case GR_RENDER_XRAY:
            // Render wireframe behind the depth map bound to sampler2D glH_DepthMap
            break;
        case GR_RENDER_POST_PASS:
            // A primitive will only get a post-pass if it requested one via
            // myInfo->requestRenderPostPass() during another rendering mode.
            // This method will return an integer ID to be checked against the
            // post pass ID (myInfo->getPostPassID()) when processing a
            // GR_RENDER_POST_PASS render. A post pass is called after all prims
            // have been rendered for the mode for which the post-pass was
            // requested. You can not request another post-pass from inside a
            // post-pass.
            // Post passes may be requested by other primitive types,
            // which is why it is important that you check the ID you received
            // from the request call against the ID of the post pass, and only
            // render if they match. The order of the post passes is determined by
            // the order in which the primitives request them, which will generally
            // be the order of the primitives in the detail (but does not have to
            // be).

            break;
        case GR_RENDER_DEPTH_CUBE:
            // Render a shadowmap pass to the 6 faces of the current
            // cube map attached to the framebuffer. Can be done using layered
            // renderer, or one by one. Depth should be the unprojected depth
            // value (eye space, linear).
            break;
        case GR_RENDER_DEPTH_LINEAR:
            // Render a linear depth should be the unprojected depth
            // value (eye space, linear). Depth map to the 2D texture attached to
            // the current framebuffer.
            break;
        case GR_RENDER_MATTE:
            // Render a constant, solid matte of the object in front of
            // the beauty pass depth texture (bound to sampler2D glH_DepthMap).
            break;
        case GR_RENDER_OBJECT_PICK:
            // render to an ivec4 framebuffer the current object ID,
            // which is bound to ivec4 glH_PickID. The fragment shader must output
            // an ivec4 with this value. The viewport has an object pick shader
            // pushed already which will do this for you if your geometry has
            // vec3 P (position) and mat4 instmat (instance/primitive transforms)
            // attributes.

            if (flags & GR_RENDER_FLAG_POINTS_ONLY)
            {
                // render the geometry points as GL_POINTS.
            }
            break;
        case GR_RENDER_SHADER_AS_IS:
            // Render the shaded geometry without setting a shader, which has
            // been set above in the callstack. Shaders require vec3 P for position,
            // and usually mat4 instmat for primitive transforms and instancing.
            if (flags & GR_RENDER_FLAG_POINTS_ONLY)
            {
                // render the geometry points as GL_POINTS.
            }
            break;
        default:
            // You shouldn't get GR_RENDER_BBOX, this is handled at the detail
            // level.
            break;
        }
    }
    int GUI_PrimFramework::renderPick(RE_RenderContext r, const GR_DisplayOption* opt, unsigned int pick_type, GR_PickStyle pick_style,
                                      bool has_pick_map)
    {
        // If you don't need your primitive to be picked on a component basis,
        // just return 0 now.

        // Called for geometry component-based picking. The pick_types are:
        //   GU_PICK_FACE        - faces, usually primitives.
        //   GU_PICK_PRIMITIVE   - primitives
        //   GU_PICK_GEOPOINT    - points
        //   GU_PICK_GEOEDGE     - edges (two connected points)
        //   GU_PICK_VERTEX      - vertices
        //   GU_PICK_BREAKPOINT  - NURBS/Bezier surface/curve breakpoints.
        //
        // Not all primitive types support these various pick types, so for example
        // if the primitive has no edges, it can exit early.

        // The pick_style determines how the picking is done.
        //   GR_PICK_SINGLE
        //      The pick IDs should be rendered to the current ivec4 framebuffer.
        //      The return value of this method is ignored.
        //
        //   GR_PICK_MULTI_VISIBLE
        //      The pick IDs should be appended to myInfo->getPickArray() (a
        //      UT_IntArray). Return the number of discrete picks made (not the
        //      number of ints added to the pick array). See below for pick
        //      structure. Element must be visible to the user. Obscured elements
        //      should not be picked.
        //
        //   GR_PICK_MULTI_FRUSTUM:
        //      Same as GR_PICK_MULTI_VISIBLE, but the element only needs to be
        //      in the selection area.

        // 'has_pick_map' indicates that lasso or paint selection is active, and
        // a 2D lum texture defining the picked area is bound to
        // sampler2D glH_PickMap. This is never true with GR_PICK_SINGLE.

        // SINGLE PICKING (click)
        //
        // For single picking, the values written to the framebuffer must be:
        //    HIGH bit                                                   LOW bits
        //    31                                                           0
        // .x [31           object ID           8] [7  geo index(high 8b)  0]
        // .y [31 geo index low 16b  16] [15         pick type             0]
        // .z [31                 component ID 1                           0]
        // .w [31                 component ID 2                           0]
        //
        // The component IDs differ depending on the pick type, and should always
        // be ** ID+1 ** (zero is reserved):
        //                          comp ID 1               comp ID 2
        // GU_PICK_PRIMITIVE        prim index                  0
        // GU_PICK_FACE             prim index                  0
        // GU_PICK_GEOPOINT         point index                 0
        // GU_PICK_GEOEDGE          edge point index #1     edge point index #2
        // GU_PICK_VERTEX           prim index              vertex index within prim
        // GU_PICK_BREAKPOINT (1D)  prim index, MSB=0       breakpoint index
        // GU_PICK_BREAKPOINT (2D)  prim index, MSB=1       U bp, 31...16 (16b)
        //                                                  V bp, 15...0  (16b)
        //
        // glH_PickID.xy already has the proper obj_id, geo_idx and pick_type set.

        // MULTI-COMPONENT PICKING: (box, lasso, paint)
        //
        // For multi picking, IDs must be appended to the pick buffer, which is
        // a UT_IntArray* accessed via r->getPickArray():
        //
        // GU_PICK_PRIMITIVE  4, obj_id, geo_idx, pick_type, prim_index
        // GU_PICK_FACE:      4, obj_id, geo_idx, pick_type, prim_index
        // GU_PICK_GEOPOINT:  4, obj_id, geo_idx, pick_type, point_index
        // GU_PICK_GEOEDGE:   6, obj_id, geo_idx, pick_type, prim_index*, pnt0, pnt1
        // GU_PICK_VERTEX:    5, obj_id, geo_idx, pick_type, prim_index, vert**
        // GU_PICK_BREAKPOINT: 5/6,obj_id,geo_idx,pick_type, prim_index, bp0 (,bp1)
        //
        // * optional, for ordered edges only
        // ** vertex index within the primitive, not GA vertex index.
        //
        // Each set counts for 1 pick when returning the number of picks from this
        // method.
        //

        // SHADER-BASED PICKING

        // Shader pick uniforms. These are defined in higher level code.
        // shader uniform type and name, (RE_UNIFORM value for r->getUniform()):
        //
        // ivec4 glH_PickID (RE_UNIFORM_PICK_ID) - .xy is set correctly already.
        // vec4  glH_PickArea (RE_UNIFORM_PICK_AREA) - pick bounding box, in pixels
        // int   glH_PickHasMap (RE_UNIFORM_PICK_HAS_MAP) - 1 if texture is present
        //                                                  for lasso, paint masks
        // sampler2D glH_PickMap (RE_UNIFORM_PICK_MAP) - sampler for 2D mask texture
        // sampler2D glH_DepthMap (RE_UNIFORM_DEPTH_MAP) - sampler for 2D depth of
        //                                                 beauty pass

        // For pick_style of GR_PICK_MULTI_*, return the number of pick entries.
        // For pick_style of GR_PICK_SINGLE, the return is ignored.
        return 0;
    }
} // namespace Zibra
