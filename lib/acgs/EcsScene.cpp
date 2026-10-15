#include "acgs/EcsScene.h"

namespace acgs
{

entt::entity SceneStore::ensure(acdb::AcDbObjectId objectId)
{
    const auto key = objectId.persistentHandle().value;
    auto found = byId_.find(key);
    if (found != byId_.end())
        return found->second;
    const entt::entity entity = registry_.create();
    byId_.emplace(key, entity);
    registry_.assign<SceneIdentity>(entity, SceneIdentity{objectId});
    ++revision_;
    return entity;
}

acdb::AcDbObjectId SceneStore::objectIdOf(entt::entity entity) const
{
    if (entity == entt::null || !registry_.has<SceneIdentity>(entity))
        return acdb::kNullObjectId;
    return registry_.get<SceneIdentity>(entity).objectId;
}

void SceneStore::forget(acdb::AcDbObjectId objectId)
{
    const auto found = byId_.find(objectId.persistentHandle().value);
    if (found == byId_.end())
        return;
    registry_.destroy(found->second);
    byId_.erase(found);
    ++revision_;
}

void SceneStore::markDirty(acdb::AcDbObjectId objectId)
{
    const entt::entity entity = ensure(objectId);
    if (registry_.has<Dirty>(entity))
        registry_.replace<Dirty>(entity);
    else
    {
        registry_.assign<Dirty>(entity);
        ++revision_;
    }
}

void SceneStore::clearDirty(acdb::AcDbObjectId objectId)
{
    const entt::entity entity = find(objectId);
    if (entity != entt::null && registry_.has<Dirty>(entity))
        registry_.remove<Dirty>(entity);
}

} // namespace acgs
