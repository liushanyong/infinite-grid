#include "acgi/AcGiDrawable.h"

#include "acdb/AcDbCore.h"
#include "acgi/AcGiWorldDraw.h"

#include <algorithm>

// AcGiGeometry::draw nesting: replays the nested drawable into this
// context (see AcGiWorldDraw.h).
Adesk::Boolean AcGiWorldDraw::draw(AcGiDrawable *nested)
{
    return nested != nullptr ? nested->worldDraw(this) : false;
}

void AcGiDrawableTraits::setFrom(const acdb::AcDbEntity &common)
{
    color_ = common.color.renderColor();
    layer_ = common.layer;
    lineType_ = common.lineType;
    lineWeight_ = common.lineWeight;
    visible_ = common.visible;
}

// ---- AcGiDrawable: sealed protocol dispatch ----

AcGiDrawable::AcGiDrawable() = default;

AcGiDrawable::~AcGiDrawable() = default;

Adesk::UInt32 AcGiDrawable::setAttributes(AcGiDrawableTraits *traits)
{
    const std::vector<AcGiDrawableOverrule *> &chain =
        AcGiDrawableOverrule::overruleChainForDispatch();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        AcGiDrawableOverrule *overrule = *it;
        if (overrule != nullptr && overrule->isApplicable(*this))
            return overrule->setAttributes(*this, traits);
    }
    return subSetAttributes(traits);
}

Adesk::Boolean AcGiDrawable::worldDraw(AcGiWorldDraw *wd)
{
    const std::vector<AcGiDrawableOverrule *> &chain =
        AcGiDrawableOverrule::overruleChainForDispatch();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        AcGiDrawableOverrule *overrule = *it;
        if (overrule != nullptr && overrule->isApplicable(*this))
            return overrule->worldDraw(*this, wd);
    }
    return subWorldDraw(wd);
}

void AcGiDrawable::viewportDraw(AcGiViewportDraw *vd)
{
    const std::vector<AcGiDrawableOverrule *> &chain =
        AcGiDrawableOverrule::overruleChainForDispatch();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        AcGiDrawableOverrule *overrule = *it;
        if (overrule != nullptr && overrule->isApplicable(*this))
        {
            overrule->viewportDraw(*this, vd);
            return;
        }
    }
    subViewportDraw(vd);
}

Adesk::UInt32 AcGiDrawable::viewportDrawLogicalFlags(AcGiViewportDraw *vd)
{
    const std::vector<AcGiDrawableOverrule *> &chain =
        AcGiDrawableOverrule::overruleChainForDispatch();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        AcGiDrawableOverrule *overrule = *it;
        if (overrule != nullptr && overrule->isApplicable(*this))
            return overrule->viewportDrawLogicalFlags(*this, vd);
    }
    return subViewportDrawLogicalFlags(vd);
}

// ---- AcGiDrawableOverrule: registry + default fall-through ----

namespace
{
std::vector<AcGiDrawableOverrule *> &overruleChain()
{
    static std::vector<AcGiDrawableOverrule *> chain;
    return chain;
}
} // namespace

AcGiDrawableOverrule::AcGiDrawableOverrule() = default;

AcGiDrawableOverrule::~AcGiDrawableOverrule() = default;

Adesk::UInt32 AcGiDrawableOverrule::setAttributes(AcGiDrawable &subject,
                                                  AcGiDrawableTraits *traits)
{
    return subject.subSetAttributes(traits);
}

Adesk::Boolean AcGiDrawableOverrule::worldDraw(AcGiDrawable &subject,
                                               AcGiWorldDraw *wd)
{
    return subject.subWorldDraw(wd);
}

void AcGiDrawableOverrule::viewportDraw(AcGiDrawable &subject,
                                        AcGiViewportDraw *vd)
{
    subject.subViewportDraw(vd);
}

Adesk::UInt32 AcGiDrawableOverrule::viewportDrawLogicalFlags(
    AcGiDrawable &subject, AcGiViewportDraw *vd)
{
    return subject.subViewportDrawLogicalFlags(vd);
}

void AcGiDrawableOverrule::addOverrule(AcGiDrawableOverrule *overrule)
{
    if (overrule == nullptr)
        return;
    std::vector<AcGiDrawableOverrule *> &chain = overruleChain();
    if (std::find(chain.begin(), chain.end(), overrule) == chain.end())
        chain.push_back(overrule);
}

void AcGiDrawableOverrule::removeOverrule(AcGiDrawableOverrule *overrule)
{
    std::vector<AcGiDrawableOverrule *> &chain = overruleChain();
    chain.erase(std::remove(chain.begin(), chain.end(), overrule),
                chain.end());
}

const std::vector<AcGiDrawableOverrule *> &
AcGiDrawableOverrule::overruleChainForDispatch()
{
    return overruleChain();
}

// ---- AcGiDrawableEvent: reactor fan-out ----

AcGiDrawableEvent::AcGiDrawableEvent() = default;

AcGiDrawableEvent &AcGiDrawableEvent::instance()
{
    static AcGiDrawableEvent event;
    return event;
}

void AcGiDrawableEvent::addReactor(AcGiDrawableReactor *reactor)
{
    if (reactor == nullptr)
        return;
    if (std::find(reactors_.begin(), reactors_.end(), reactor) ==
        reactors_.end())
        reactors_.push_back(reactor);
}

void AcGiDrawableEvent::removeReactor(AcGiDrawableReactor *reactor)
{
    reactors_.erase(
        std::remove(reactors_.begin(), reactors_.end(), reactor),
        reactors_.end());
}

void AcGiDrawableEvent::sendBeginRegen(AcGiDrawable *drawable,
                                       AcGiDrawableReactor::RegenFlags flags)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->beginRegen(drawable, flags);
}

void AcGiDrawableEvent::sendEndRegen(AcGiDrawable *drawable,
                                     AcGiDrawableReactor::RegenFlags flags)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->endRegen(drawable, flags);
}

void AcGiDrawableEvent::sendModified(AcGiDrawable *drawable)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->modified(drawable);
}

void AcGiDrawableEvent::sendErased(AcGiDrawable *drawable)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->erased(drawable);
}

void AcGiDrawableEvent::sendHandOverTo(AcGiDrawable *drawableFrom,
                                       AcGiDrawable *drawableTo)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->handOverTo(drawableFrom, drawableTo);
}

void AcGiDrawableEvent::sendAdded(AcGiDrawable *drawable)
{
    for (AcGiDrawableReactor *reactor : reactors_)
        reactor->added(drawable);
}
