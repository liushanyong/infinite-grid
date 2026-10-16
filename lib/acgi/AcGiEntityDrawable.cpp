#include "acgi/AcGiEntityDrawable.h"

namespace acgi
{

std::unique_ptr<AcGiDrawable>
makeLeafDrawable(const acdb::AcDbEntityVariant &payload, std::string name)
{
    return std::make_unique<AcGiVariantDrawable>(payload, std::move(name));
}

std::unique_ptr<AcGiDrawable>
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

AcGiEntityIdentityDrawable::AcGiEntityIdentityDrawable(
    const acdb::AcDbEntity &identity, std::string name)
    : identity_(identity),
      name_(name.empty() ? identity.name : std::move(name))
{
}

Adesk::Boolean AcGiEntityIdentityDrawable::isPersistent(void) const
{
    return identity_.handle.isValid();
}

acdb::AcDbObjectId AcGiEntityIdentityDrawable::id(void) const
{
    return isPersistent() ? acdb::AcDbObjectId(identity_.handle)
                          : acdb::AcDbObjectId();
}

const std::string &AcGiEntityIdentityDrawable::name() const
{
    return identity_.name;
}

Adesk::UInt32 AcGiEntityIdentityDrawable::subSetAttributes(
    AcGiDrawableTraits *traits)
{
    if (traits != nullptr)
        traits->setFrom(identity_);
    return kDrawableIsAnEntity;
}

AcGiVariantDrawable::AcGiVariantDrawable(
    const acdb::AcDbEntityVariant &payload, std::string name)
    : AcGiEntityIdentityDrawable(acdb::common(payload), std::move(name)),
      payload_(payload)
{
}

Adesk::Boolean AcGiVariantDrawable::subWorldDraw(AcGiWorldDraw *wd)
{
    return wd != nullptr && acgi::entityDrawable(payload_)->worldDraw(wd);
}

void AcGiVariantDrawable::subViewportDraw(AcGiViewportDraw *vd)
{
    if (vd != nullptr)
        acgi::entityDrawable(payload_)->viewportDraw(vd);
}

AcGiCompoundDrawable::AcGiCompoundDrawable(
    const acdb::AcDbEntity &identity, std::string name)
    : AcGiEntityIdentityDrawable(identity, std::move(name))
{
}

Adesk::UInt32 AcGiCompoundDrawable::subSetAttributes(
    AcGiDrawableTraits *traits)
{
    AcGiEntityIdentityDrawable::subSetAttributes(traits);
    return kDrawableIsAnEntity | kDrawableIsCompoundObject;
}

Adesk::Boolean AcGiCompoundDrawable::subWorldDraw(AcGiWorldDraw *wd)
{
    if (wd == nullptr)
        return false;
    for (const Member &member : members_)
        if (member.drawable != nullptr)
            wd->draw(member.drawable.get());
    return true;
}

void AcGiCompoundDrawable::subViewportDraw(AcGiViewportDraw *vd)
{
    subWorldDraw(vd);
}

const std::vector<AcGiCompoundDrawable::Member> &
AcGiCompoundDrawable::members() const
{
    return members_;
}

void AcGiCompoundDrawable::appendInstanceAttributes(
    const acdb::AcDbDatabase &document,
    const std::vector<acdb::AcDbHandle> &attributeHandles,
    const std::string &instanceName, const AcGePoint3d &originWorld)
{
    for (const acdb::AcDbHandle handle : attributeHandles)
    {
        const acdb::AcDbEntityVariant *payload = document.getEntity(handle);
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

AcGiBlockReferenceDrawable::AcGiBlockReferenceDrawable(
    const acdb::AcDbDatabase &document, acdb::AcDbHandle insertHandle,
    const acdb::AcDbBlockReference &reference)
    : AcGiCompoundDrawable(reference)
{
    const glm::dmat4 topWorld = glm::dmat4(document.referenceTransform(reference));
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
          members_.push_back({acgi::makeLeafDrawable(instance),
                              acdb::common(instance).name});
        });
    appendInstanceAttributes(document, reference.attributeHandles,
                             instanceName, reference.position);
}

AcGiMInsertBlockDrawable::AcGiMInsertBlockDrawable(
    const acdb::AcDbDatabase &document, acdb::AcDbHandle insertHandle,
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
