#pragma once

// GS-side replay helpers on top of the IS-A drawable hierarchy.
//
// AcDbEntity derives AcGiDrawable (AcDbCore.h), so the variant-held
// entities ARE drawables: acgi::entityDrawable hands them to the sealed
// protocol directly — the ObjectARX AcGsModel::add(pEntity) shape,
// non-owning.  What remains here is the Gs replay's own kit:
//
//   * AcGiVariantDrawable — an OWNING drawable over an entity copy
//     (compound members: instance copies with composed placement),
//   * AcGiCompoundDrawable + AcGiBlockReferenceDrawable /
//     AcGiMInsertBlockDrawable — block expansion through
//     AcGiWorldDraw::draw nesting (the database walk needs the document,
//     which stays a replay-side input).

#include "acgi/AcGiDrawable.h"

#include "acdb/AcDbDatabase.h"
#include "acdb/AcDbEntities.h"
#include "acdb/AcDbTransform.h"
#include "acgi/AcGiTextEngine.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace acgi
{

// Per-entity replay inputs that are GS decisions rather than entity data.
struct AcGiDrawableContext
{
    const acdb::AcDbDatabase *document = nullptr;
    acdb::AcDbHandle handle{};
};

// The variant-held entity IS the drawable (ARX IS-A).  Non-owning view;
// the regen protocol does not mutate the entity, so casting away const on
// a document read is protocol-safe.
inline AcGiDrawable *entityDrawable(acdb::AcDbEntityVariant &payload)
{
    return std::visit(
        [](auto &entity) -> AcGiDrawable * { return &entity; }, payload);
}

inline AcGiDrawable *entityDrawable(const acdb::AcDbEntityVariant &payload)
{
    return entityDrawable(
        const_cast<acdb::AcDbEntityVariant &>(payload));
}

// Leaf factory for compound members (owning copy); defined at the bottom
// of this header once AcGiVariantDrawable is complete.
ACGI_PORT std::unique_ptr<AcGiDrawable>
makeLeafDrawable(const acdb::AcDbEntityVariant &payload,
                 std::string name = std::string());

} // namespace acgi

// Identity for GS-side drawables that stand for an entity: handle + name
// copied from the entity common.
class ACGI_PORT AcGiEntityIdentityDrawable : public AcGiDrawable
{
public:
    Adesk::Boolean isPersistent(void) const override;
    acdb::AcDbObjectId id(void) const override;
    const std::string &name() const;

protected:
    explicit AcGiEntityIdentityDrawable(const acdb::AcDbEntity &identity,
                                        std::string name = std::string());

    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;

    acdb::AcDbEntity identity_;
    std::string name_;
};

// Owning drawable over an entity copy — compound members (block instance
// copies with composed placement baked in).  Forwards the sealed protocol
// to the held entity, which draws itself.
class ACGI_PORT AcGiVariantDrawable : public AcGiEntityIdentityDrawable
{
public:
    explicit AcGiVariantDrawable(const acdb::AcDbEntityVariant &payload,
                                 std::string name = std::string());

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    void subViewportDraw(AcGiViewportDraw *vd) override;

private:
    acdb::AcDbEntityVariant payload_;
};

// ---- AcGiCompoundDrawable: drawables that nest child drawables ----
//
// ObjectARX kDrawableIsCompoundObject: blocks nest their members through
// AcGiGeometry::draw and send no primitives of their own.  The Gs replay
// also walks the children directly when it records per-member metafile
// ranges (block members stay individually named and pickable).

class ACGI_PORT AcGiCompoundDrawable : public AcGiEntityIdentityDrawable
{
public:
    struct Member
    {
        std::unique_ptr<AcGiDrawable> drawable;
        std::string name;
    };

    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override;
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override;
    void subViewportDraw(AcGiViewportDraw *vd) override;
    const std::vector<Member> &members() const;

protected:
    explicit AcGiCompoundDrawable(const acdb::AcDbEntity &identity,
                                  std::string name = std::string());

    // Attribute values stamped on the instance draw under it: a copy
    // translated to the instance origin, named instanceName/tag.
    void appendInstanceAttributes(
        const acdb::AcDbDatabase &document,
        const std::vector<acdb::AcDbHandle> &attributeHandles,
        const std::string &instanceName, const AcGePoint3d &originWorld);

    std::vector<Member> members_;
};

// ---- AcGiBlockReferenceDrawable: expanded block instance ----
//
// The referenced block table record walks through
// AcDbDatabase::walkInsertInstances — one member drawable per non-INSERT
// member with its composed placement baked in (the replay policy: instance
// copies carry the composed origin, so member geometry degrades through
// the sampled forms and stays lossy by design).

class ACGI_PORT AcGiBlockReferenceDrawable : public AcGiCompoundDrawable
{
public:
    AcGiBlockReferenceDrawable(const acdb::AcDbDatabase &document,
                               acdb::AcDbHandle insertHandle,
                               const acdb::AcDbBlockReference &reference);
};

// ---- AcGiMInsertBlockDrawable: the same expansion per grid cell ----
//
// The cell offset rotates with the reference (spacings are authored in
// drawing units along the insert's own axes).

class ACGI_PORT AcGiMInsertBlockDrawable : public AcGiCompoundDrawable
{
public:
    AcGiMInsertBlockDrawable(const acdb::AcDbDatabase &document,
                             acdb::AcDbHandle insertHandle,
                             const acdb::AcDbMInsertBlock &insert);
};

namespace acgi
{

// Leaf factory for compound members: an owning drawable over the instance
// copy (a nested INSERT-like payload draws as its own leaf form — the
// replay never re-expands inside a member walk).
ACGI_PORT std::unique_ptr<AcGiDrawable>
makeLeafDrawable(const acdb::AcDbEntityVariant &payload, std::string name);

// The Gs replay factory: INSERT/MINSERT become compound drawables (their
// expansion needs the document).  Everything else — text entities
// included — IS the drawable; the caller uses entityDrawable(payload) and
// the entity draws itself through its own subWorldDraw.
ACGI_PORT std::unique_ptr<AcGiDrawable>
makeOwningEntityDrawable(const acdb::AcDbEntityVariant &payload,
                         const AcGiDrawableContext &context);

} // namespace acgi
