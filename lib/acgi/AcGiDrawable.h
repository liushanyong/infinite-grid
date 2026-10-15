#pragma once

// AcGiDrawable — the regen-protocol root, ported from the ObjectARX
// drawable.h layout (see lib/objectArx-inc/drawable.h / arxref.chm,
// "AcGiDrawable").  ObjectARX: every graphical object derives
// AcGiDrawable and the graphics system drives it through the sealed
// setAttributes -> worldDraw / viewportDraw sequence, recording the emitted
// primitives into its metafile.
//
// In ObjectARX the entity classes ARE drawables: AcDbEntity derives
// AcDbObject and AcGiDrawable, so AcDbPolyFaceMesh (and every sibling)
// is-a AcGiDrawable.  This port reproduces the IS-A chain: acdb::AcDbEntity
// derives this class and the concrete entity payloads override the sub*
// hooks; the graphics system (acgs::AcGsDocumentReplayer) drives them
// through the sealed protocol.
//
// Deliberate deviations from drawable.h, both consequences of the
// value-type layer (no AcRx heap-object graph):
//   * No AcRxObject base and no ACRX_DECLARE_MEMBERS class registry; the
//     class is a plain polymorphic root.
//   * The "sealed" methods are non-virtual dispatchers: they run the
//     AcGiDrawableOverrule chain, then forward to the protected sub* hook.
//   * id() returns the persistent identity by value
//     (acdb::AcDbObjectId; null when transient) instead of a heap pointer.
//   * AcDbEntity implements the sub* hooks with defaults (the base stays
//     concrete: soup records embed it by value); ARX keeps them pure.
//   * RolloverHit / setDrawStream are omitted until those subsystems exist.
//
// This header must stay dependency-light: it is included by AcDbCore.h
// (the entity base), so it only forward-declares the draw contexts and
// pulls identity from acdb/AcDbHandle.h.  The AcGiWorldDraw nesting hook
// (draw) is defined at the bottom, where both sides are complete.

#include "AcGiPort.h"

#include "arx/ArxTypes.h"

#include "../acge/AcGe.h"
#include "acdb/AcDbHandle.h"

#include <string>
#include <vector>

class AcGiWorldDraw;
class AcGiViewportDraw;
class AcGiDrawableOverrule;
class AcGiDrawableReactor;

namespace acdb
{
class AcDbEntity;
} // namespace acdb

// The abstract curve base is not ported yet (acge ships concrete value
// types); the generic-curve callback binds to the future class through the
// forward declaration.
struct AcGeCurve3d;

// ---- AcDbExtents: the bounds() carrier (ObjectARX dbmain.h AcDbExtents) ----

class AcDbExtents : public AcGeBoundBlock3d
{
};

// ---- AcGiDrawableTraits: the attribute carrier handed to setAttributes ----
//
// ObjectARX AcGiDrawableTraits subset.  Carries the render-visible property
// set so a drawable's setAttributes output can flow into the geometry sink.

class ACGI_PORT AcGiDrawableTraits
{
public:
    void setColor(const glm::vec4 &color) { color_ = color; }
    void setLayer(std::string layer) { layer_ = std::move(layer); }
    void setLineType(std::string lineType)
    {
        lineType_ = std::move(lineType);
    }
    void setLineWeight(double lineWeight) { lineWeight_ = lineWeight; }
    void setVisibility(bool visible) { visible_ = visible; }

    // Applies the entity-common attribute set in one call (defined in
    // AcGiDrawable.cpp; acdb::AcDbEntity is incomplete here).
    void setFrom(const acdb::AcDbEntity &common);

    [[nodiscard]] const glm::vec4 &color() const { return color_; }
    [[nodiscard]] const std::string &layer() const { return layer_; }
    [[nodiscard]] const std::string &lineType() const { return lineType_; }
    [[nodiscard]] double lineWeight() const { return lineWeight_; }
    [[nodiscard]] bool visibility() const { return visible_; }

private:
    glm::vec4 color_{1.0f, 1.0f, 1.0f, 1.0f};
    std::string layer_ = "0";
    std::string lineType_ = "ByLayer";
    double lineWeight_ = 0.0;
    bool visible_ = true;
};

class ACGI_PORT AcGiDrawable
{
public:

    // Combine these flags in the return value of setAttributes.  The
    // graphics system configures subsequent graphics from them; use the
    // lowest value appropriate (see drawable.h SetAttributesFlags).
    enum SetAttributesFlags
    {
        kDrawableNone                           =  0,
        kDrawableIsAnEntity                     =  1,
        kDrawableUsesNesting                    =  2,
        kDrawableIsCompoundObject               =  4,
        kDrawableViewIndependentViewportDraw    =  8,
        kDrawableIsInvisible                    =  16,
        kDrawableHasAttributes                  =  32,
        kDrawableRegenTypeDependentGeometry     =  64,
        kDrawableIsDimension = (kDrawableIsAnEntity + kDrawableIsCompoundObject + 128),
        kDrawableRegenDraw                      = 256,
        kDrawableStandardDisplaySingleLOD       = 512,
        kDrawableShadedDisplaySingleLOD         = 1024,
        kDrawableViewDependentViewportDraw      =  2048,
        kDrawableBlockDependentViewportDraw     =  4096,
        kDrawableIsExternalReference            =  8192,
        kDrawableNotPlottable                   = 16384,
        kDrawableNotAllowLCS                    = 32768,
        kDrawableMergeControlOff                = 65536,
        kThreadedWorldDrawViewportDraw          = 131072,
        kDrawableManagesChildDrawables          = 262144,
        kDrawableInheritsSelectionMarker        = 524288
    };

    // These are the Drawable types (drawable.h DrawableType).
    enum DrawableType
    {
        kGeometry              = 0,
        kDistantLight,
        kPointLight,
        kSpotLight,
        kAmbientLight,
        kSolidBackground,
        kGradientBackground,
        kImageBackground,
        kGroundPlaneBackground,
        kViewport,
        kWebLight,
        kSkyBackground,
        kImageBasedLightingBackground
    };

    ACGI_PORT AcGiDrawable();
    ACGI_PORT virtual ~AcGiDrawable();

    // Sealed protocol entry points: overrule chain first, then the sub*
    // hook.  The Gs replay calls exactly these.
    ACGI_PORT Adesk::UInt32 setAttributes(AcGiDrawableTraits *traits);
    ACGI_PORT Adesk::Boolean worldDraw(AcGiWorldDraw *wd);
    ACGI_PORT void viewportDraw(AcGiViewportDraw *vd);
    ACGI_PORT Adesk::UInt32 viewportDrawLogicalFlags(AcGiViewportDraw *vd);

    // Persistent/transient identity.
    virtual Adesk::Boolean isPersistent(void) const = 0;
    virtual acdb::AcDbObjectId id(void) const = 0;

    // Drawable type.
    virtual DrawableType drawableType(void) const { return kGeometry; }

    // Set the bounds of the drawable.  Return false if the drawable has no
    // bounds, the bounds cannot be set, or the method is unimplemented.
    virtual bool bounds(AcDbExtents &bounds) const
    {
        (void)bounds;
        return false;
    }

protected:
    friend class AcGiDrawableOverrule;

    virtual Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) = 0;
    virtual Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) = 0;
    virtual void subViewportDraw(AcGiViewportDraw *vd) = 0;
    virtual Adesk::UInt32 subViewportDrawLogicalFlags(AcGiViewportDraw *)
    {
        return 0;
    }
};

// ---- AcGiDrawableOverrule ----
//
// Overrules a subset of the draw-related operations AcGiDrawable specifies
// (drawable.h AcGiDrawableOverrule).  Registered overrules are consulted
// newest-first; the first isApplicable() hit serves the call.  The default
// implementations forward to the subject's protected sub* hook, so an
// overrule that only tweaks some operations falls through for the rest by
// calling them.  This is the extension seam for altering how a drawable
// renders without touching its class.

class ACGI_PORT AcGiDrawableOverrule
{
public:
    ACGI_PORT AcGiDrawableOverrule();
    ACGI_PORT virtual ~AcGiDrawableOverrule();

    virtual bool isApplicable(const AcGiDrawable &subject) const
    {
        (void)subject;
        return true;
    }

    ACGI_PORT virtual Adesk::UInt32 setAttributes(AcGiDrawable &subject,
                                                  AcGiDrawableTraits *traits);
    ACGI_PORT virtual Adesk::Boolean worldDraw(AcGiDrawable &subject,
                                               AcGiWorldDraw *wd);
    ACGI_PORT virtual void viewportDraw(AcGiDrawable &subject,
                                        AcGiViewportDraw *vd);
    ACGI_PORT virtual Adesk::UInt32 viewportDrawLogicalFlags(
        AcGiDrawable &subject, AcGiViewportDraw *vd);

    // Registry: raw pointers; the owner unregisters before destruction.
    ACGI_PORT static void addOverrule(AcGiDrawableOverrule *overrule);
    ACGI_PORT static void removeOverrule(AcGiDrawableOverrule *overrule);

private:
    // Dispatch reads the chain through this accessor (function-local static
    // lives in the implementation file).
    ACGI_PORT static const std::vector<AcGiDrawableOverrule *> &
    overruleChainForDispatch();

    friend class AcGiDrawable;
};

// ---- AcGiDrawableReactor / AcGiDrawableEvent ----
//
// Notifications of key drawable events — regen begin/end, modification,
// erasure (drawable.h AcGiDrawableReactor / AcGiDrawableEvent).  Clients
// derive from the reactor, register with the singleton below, and receive
// the sends; the Gs replay fires sendModified per regenerated drawable.
// The parentID / databasePtr overloads of the ObjectARX reactor are folded
// into the base signatures until a consumer needs them.

class ACGI_PORT AcGiDrawableReactor
{
public:
    enum RegenFlags
    {
        k2DRegen,
        k3DRegen
    };

    virtual ~AcGiDrawableReactor() = default;

    virtual void beginRegen(AcGiDrawable *, RegenFlags) {}
    virtual void endRegen(AcGiDrawable *, RegenFlags) {}
    virtual void modified(AcGiDrawable *) {}
    virtual void erased(AcGiDrawable *) {}
    virtual void handOverTo(AcGiDrawable *, AcGiDrawable *) {}
    virtual void added(AcGiDrawable *) {}
};

#define acgiDrawableEvent (&AcGiDrawableEvent::instance())

class ACGI_PORT AcGiDrawableEvent
{
public:
    // The one AcGiDrawableEvent; use the acgiDrawableEvent macro.
    ACGI_PORT static AcGiDrawableEvent &instance();

    ACGI_PORT void addReactor(AcGiDrawableReactor *reactor);
    ACGI_PORT void removeReactor(AcGiDrawableReactor *reactor);

    ACGI_PORT void sendBeginRegen(AcGiDrawable *drawable,
                                  AcGiDrawableReactor::RegenFlags flags);
    ACGI_PORT void sendEndRegen(AcGiDrawable *drawable,
                                AcGiDrawableReactor::RegenFlags flags);
    ACGI_PORT void sendModified(AcGiDrawable *drawable);
    ACGI_PORT void sendErased(AcGiDrawable *drawable);
    ACGI_PORT void sendHandOverTo(AcGiDrawable *drawableFrom,
                                  AcGiDrawable *drawableTo);
    ACGI_PORT void sendAdded(AcGiDrawable *drawable);

private:
    AcGiDrawableEvent();
    AcGiDrawableEvent(const AcGiDrawableEvent &) = delete;
    AcGiDrawableEvent &operator=(const AcGiDrawableEvent &) = delete;

    std::vector<AcGiDrawableReactor *> reactors_;
};
