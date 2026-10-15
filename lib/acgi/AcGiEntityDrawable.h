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
std::unique_ptr<AcGiDrawable>
makeLeafDrawable(const acdb::AcDbEntityVariant &payload,
                 std::string name = std::string());

} // namespace acgi

// Identity for GS-side drawables that stand for an entity: handle + name
// copied from the entity common.
class AcGiEntityIdentityDrawable : public AcGiDrawable
{
public:
    Adesk::Boolean isPersistent(void) const override
    {
        return identity_.handle.isValid();
    }

    acdb::AcDbObjectId id(void) const override
    {
        return isPersistent() ? acdb::AcDbObjectId(identity_.handle)
                              : acdb::AcDbObjectId();
    }

    const std::string &name() const { return identity_.name; }

protected:
    explicit AcGiEntityIdentityDrawable(const acdb::AcDbEntity &identity,
                                        std::string name = std::string())
        : identity_(identity),
          name_(name.empty() ? identity.name : std::move(name))
    {
    }

    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override
    {
        if (traits != nullptr)
            traits->setFrom(identity_);
        return kDrawableIsAnEntity;
    }

    acdb::AcDbEntity identity_;
    std::string name_;
};

// Owning drawable over an entity copy — compound members (block instance
// copies with composed placement baked in).  Forwards the sealed protocol
// to the held entity, which draws itself.
class AcGiVariantDrawable : public AcGiEntityIdentityDrawable
{
public:
    explicit AcGiVariantDrawable(const acdb::AcDbEntityVariant &payload,
                                 std::string name = std::string())
        : AcGiEntityIdentityDrawable(acdb::common(payload), std::move(name)),
          payload_(payload)
    {
    }

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override
    {
        return wd != nullptr &&
               acgi::entityDrawable(payload_)->worldDraw(wd);
    }

    void subViewportDraw(AcGiViewportDraw *vd) override
    {
        if (vd != nullptr)
            acgi::entityDrawable(payload_)->viewportDraw(vd);
    }

private:
    acdb::AcDbEntityVariant payload_;
};

// ---- AcGiCompoundDrawable: drawables that nest child drawables ----
//
// ObjectARX kDrawableIsCompoundObject: blocks nest their members through
// AcGiGeometry::draw and send no primitives of their own.  The Gs replay
// also walks the children directly when it records per-member metafile
// ranges (block members stay individually named and pickable).

class AcGiCompoundDrawable : public AcGiEntityIdentityDrawable
{
public:
    struct Member
    {
        std::unique_ptr<AcGiDrawable> drawable;
        std::string name;
    };

    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits *traits) override
    {
        AcGiEntityIdentityDrawable::subSetAttributes(traits);
        return kDrawableIsAnEntity | kDrawableIsCompoundObject;
    }

    Adesk::Boolean subWorldDraw(AcGiWorldDraw *wd) override
    {
        if (wd == nullptr)
            return false;
        for (const Member &member : members_)
            if (member.drawable != nullptr)
                wd->draw(member.drawable.get());
        return true;
    }

    void subViewportDraw(AcGiViewportDraw *vd) override { subWorldDraw(vd); }

    const std::vector<Member> &members() const { return members_; }

protected:
    explicit AcGiCompoundDrawable(const acdb::AcDbEntity &identity,
                                  std::string name = std::string())
        : AcGiEntityIdentityDrawable(identity, std::move(name))
    {
    }

    // Attribute values stamped on the instance draw under it: a copy
    // translated to the instance origin, named instanceName/tag.
    void appendInstanceAttributes(
        const acdb::AcDbDatabase &document,
        const std::vector<acdb::AcDbHandle> &attributeHandles,
        const std::string &instanceName, const AcGePoint3d &originWorld)
    {
        for (const acdb::AcDbHandle handle : attributeHandles)
        {
            const acdb::AcDbEntityVariant *payload =
                document.getEntity(handle);
            if (payload == nullptr || document.isErased(handle))
                continue;
            const auto *attribute = std::get_if<acdb::AcDbAttribute>(payload);
            if (attribute == nullptr)
                continue;
            acdb::AcDbAttribute instance = *attribute;
            instance.insertion += originWorld.asVector();
            instance.name =
                instanceName + "/" +
                (attribute->tag.empty() ? "ATT" : attribute->tag);
            members_.push_back(
                {acgi::makeLeafDrawable(acdb::AcDbEntityVariant(instance)),
                 instance.name});
        }
    }

    std::vector<Member> members_;
};

// ---- AcGiBlockReferenceDrawable: expanded block instance ----
//
// The referenced block table record walks through
// AcDbDatabase::walkInsertInstances — one member drawable per non-INSERT
// member with its composed placement baked in (the replay policy: instance
// copies carry the composed origin, so member geometry degrades through
// the sampled forms and stays lossy by design).

class AcGiBlockReferenceDrawable : public AcGiCompoundDrawable
{
public:
    AcGiBlockReferenceDrawable(const acdb::AcDbDatabase &document,
                               acdb::AcDbHandle insertHandle,
                               const acdb::AcDbBlockReference &reference)
        : AcGiCompoundDrawable(reference)
    {
        const glm::dmat4 topWorld =
            glm::dmat4(document.referenceTransform(reference));
        const std::string instanceName =
            reference.name + "@" + std::to_string(insertHandle.value);
        document.walkInsertInstances(
            reference.blockTableRecordName, topWorld,
            [&](const glm::dmat4 &world, acdb::AcDbHandle memberHandle,
                const acdb::AcDbEntityVariant &payload) {
              (void)memberHandle;
              const glm::dvec3 origin =
                  glm::dvec3(world * glm::dvec4(0.0, 0.0, 0.0, 1.0));
              acdb::AcDbEntityVariant instance = payload;
              acdb::transformBy(instance, origin);
              acdb::common(instance).name =
                  instanceName + "/" + acdb::common(payload).name;
              members_.push_back(
                  {acgi::makeLeafDrawable(instance),
                   acdb::common(instance).name});
            });
        appendInstanceAttributes(document, reference.attributeHandles,
                                 instanceName, reference.position);
    }
};

// ---- AcGiMInsertBlockDrawable: the same expansion per grid cell ----
//
// The cell offset rotates with the reference (spacings are authored in
// drawing units along the insert's own axes).

class AcGiMInsertBlockDrawable : public AcGiCompoundDrawable
{
public:
    AcGiMInsertBlockDrawable(const acdb::AcDbDatabase &document,
                             acdb::AcDbHandle insertHandle,
                             const acdb::AcDbMInsertBlock &insert)
        : AcGiCompoundDrawable(insert)
    {
        const std::string instanceName =
            insert.name + "@" + std::to_string(insertHandle.value);
        const AcGeVector3d axis =
            insert.normal.lengthSq() > 1.0e-12
                ? insert.normal.normal()
                : AcGeVector3d(0.0, 0.0, 1.0);
        const AcGeMatrix3d rotation =
            insert.rotation != 0.0
                ? AcGeMatrix3d::setToRotation(insert.rotation, axis,
                                              AcGePoint3d(0.0, 0.0, 0.0))
                : AcGeMatrix3d();
        const int rows = std::clamp(insert.rowCount, 1, 32768);
        const int columns = std::clamp(insert.columnCount, 1, 32768);
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const AcGeVector3d offset = transformBy(
                    AcGeVector3d(double(column) * insert.columnSpacing,
                                 double(row) * insert.rowSpacing, 0.0),
                    rotation);
                acdb::AcDbBlockReference cell;
                cell.blockTableRecordName = insert.blockTableRecordName;
                cell.position = insert.position + offset;
                cell.rotation = insert.rotation;
                cell.scale = insert.scale;
                cell.normal = insert.normal;
                const glm::dmat4 cellWorld =
                    glm::dmat4(document.referenceTransform(cell));
                document.walkInsertInstances(
                    insert.blockTableRecordName, cellWorld,
                    [&](const glm::dmat4 &world,
                        acdb::AcDbHandle memberHandle,
                        const acdb::AcDbEntityVariant &payload) {
                      (void)memberHandle;
                      const glm::dvec3 origin =
                          glm::dvec3(world * glm::dvec4(0.0, 0.0, 0.0, 1.0));
                      acdb::AcDbEntityVariant instance = payload;
                      acdb::transformBy(instance, origin);
                      acdb::common(instance).name =
                          instanceName + "/" + acdb::common(payload).name;
                      members_.push_back(
                          {acgi::makeLeafDrawable(instance),
                           acdb::common(instance).name});
                    });
                appendInstanceAttributes(document, insert.attributeHandles,
                                         instanceName, cell.position);
            }
        }
    }
};

namespace acgi
{

// Leaf factory for compound members: an owning drawable over the instance
// copy (a nested INSERT-like payload draws as its own leaf form — the
// replay never re-expands inside a member walk).
inline std::unique_ptr<AcGiDrawable>
makeLeafDrawable(const acdb::AcDbEntityVariant &payload, std::string name)
{
    return std::make_unique<AcGiVariantDrawable>(payload, std::move(name));
}

// The Gs replay factory: INSERT/MINSERT become compound drawables (their
// expansion needs the document).  Everything else — text entities
// included — IS the drawable; the caller uses entityDrawable(payload) and
// the entity draws itself through its own subWorldDraw.
inline std::unique_ptr<AcGiDrawable>
makeOwningEntityDrawable(const acdb::AcDbEntityVariant &payload,
                         const AcGiDrawableContext &context)
{
    if (context.document != nullptr)
    {
        if (const auto *reference =
                std::get_if<acdb::AcDbBlockReference>(&payload))
        {
            return std::make_unique<AcGiBlockReferenceDrawable>(
                *context.document, context.handle, *reference);
        }
        if (const auto *gridInsert =
                std::get_if<acdb::AcDbMInsertBlock>(&payload))
        {
            return std::make_unique<AcGiMInsertBlockDrawable>(
                *context.document, context.handle, *gridInsert);
        }
    }
    return nullptr;
}

} // namespace acgi
