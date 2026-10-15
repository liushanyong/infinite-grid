#include "acgs/DocumentSceneBridge.h"

#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace acgs
{

DocumentSceneBridge::DocumentSceneBridge(acdb::AcDbDatabase &document,
                                         SceneStore &store)
    : document_(document), store_(store)
{
    document_.addReactor(this);
}

DocumentSceneBridge::~DocumentSceneBridge()
{
    document_.removeReactor(this);
}

void DocumentSceneBridge::open()
{
    std::unordered_set<std::uint64_t> residents;
    document_.forEachEntity([&](acdb::AcDbHandle handle,
                                const acdb::AcDbEntityVariant &) {
        residents.insert(handle.value);
        store_.ensure(acdb::AcDbObjectId{handle});
        attachAttributes(handle);
        store_.markDirty(acdb::AcDbObjectId{handle});
    });
    document_.forEachNonGraphicalObject(
        [&](acdb::AcDbHandle handle,
            const acdb::AcDbNonGraphicalObject &) {
            residents.insert(handle.value);
            store_.ensure(acdb::AcDbObjectId{handle});
        });
    std::vector<acdb::AcDbObjectId> stale;
    store_.forEachObjectId([&](acdb::AcDbObjectId objectId) {
        if (!residents.contains(objectId.persistentHandle().value))
            stale.push_back(objectId);
    });
    for (const acdb::AcDbObjectId objectId : stale)
        store_.forget(objectId);
}

void DocumentSceneBridge::objectAppended(const acdb::AcDbDatabase &,
                                         acdb::AcDbHandle handle)
{
    store_.ensure(acdb::AcDbObjectId{handle});
    attachAttributes(handle);
}

void DocumentSceneBridge::objectErased(const acdb::AcDbDatabase &,
                                       acdb::AcDbHandle handle)
{
    store_.markDirty(acdb::AcDbObjectId{handle});
}

void DocumentSceneBridge::objectUnerased(const acdb::AcDbDatabase &,
                                         acdb::AcDbHandle handle)
{
    attachAttributes(handle);
    store_.markDirty(acdb::AcDbObjectId{handle});
}

void DocumentSceneBridge::objectRemoved(const acdb::AcDbDatabase &,
                                        acdb::AcDbHandle handle)
{
    store_.forget(acdb::AcDbObjectId{handle});
}

void DocumentSceneBridge::objectModified(const acdb::AcDbDatabase &,
                                         acdb::AcDbHandle handle)
{
    attachAttributes(handle);
    store_.markDirty(acdb::AcDbObjectId{handle});
}

void DocumentSceneBridge::databaseReplaced(const acdb::AcDbDatabase &)
{
    open();
}

void DocumentSceneBridge::attachAttributes(acdb::AcDbHandle handle)
{
    const entt::entity entity = store_.find(acdb::AcDbObjectId{handle});
    if (entity == entt::null)
        return;
    const acdb::AcDbEntityVariant *payload =
        document_.getEntity(handle);
    if (payload == nullptr)
        return;
    entt::registry &registry = store_.registry();
    const auto assignOrReplace = [&](auto &&component) {
        using Component =
            std::decay_t<decltype(component)>;
        if (registry.has<Component>(entity))
            registry.replace<Component>(
                entity, std::forward<decltype(component)>(component));
        else
            registry.assign<Component>(
                entity, std::forward<decltype(component)>(component));
    };
    assignOrReplace(LayerRef{acdb::common(*payload).getLayerName()});
    if (const auto *reference =
            std::get_if<acdb::AcDbBlockReference>(payload))
    {
        assignOrReplace(
            InstanceXform{document_.referenceTransform(*reference)});
    }
    else if (registry.has<InstanceXform>(entity))
    {
        registry.remove<InstanceXform>(entity);
    }
}

} // namespace acgs
