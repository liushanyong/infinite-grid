#pragma once

// acgs::SceneStore (namespace acgs) — the ECS scene layer the roadmap
// assigns to attributes/scene/selection ("拓扑用紧凑 arena，属性/场景/
// 选择/撤销才上 ECS").  entt-backed; the in-memory identity split is
// "SQLite 管理 AcDbHandle 管文件，ECS 管理 AcDbObjectId 管内存":
// every ECS entity is keyed one-to-one by an acdb::AcDbObjectId (which
// today wraps its persistent AcDbHandle 1:1), so the ECS never becomes
// a second ID space and the persistent/file side never leaks entt
// types ("全链路 ID 化禁传指针").  The brep topology layer keeps its
// arena and stays out of here.
//
// v1 components:
//   SceneIdentity  the owning AcDbObjectId (carried on every ECS
//                  entity; the persistent handle stays reachable
//                  through objectId.persistentHandle())
//   Drawable       render-side identity (pickId feeding the GPU ID
//                  buffer; assigned by the graphics side, independent
//                  of the document identity)
//   Dirty          rebuild flag consumed by the batch-sync system
//   InstanceXform  world transform for block-reference instances
//   LayerRef       resolved layer name (attribute layer)
//
// The document keeps this mirror in step through
// acgs::DocumentSceneBridge (AcDbDatabaseReactor); direct ensure/
// forget callers exist only for documents without a bound bridge.
//
// Selection stays in AcGsSelectionManager for now; migrating it onto
// Selected tags is a follow-up.

#include <cstdint>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "acdb/AcDbCore.h"
#include "ge/gematrix.h"

namespace acgs
{

struct SceneIdentity
{
    acdb::AcDbObjectId objectId;
};

struct Dirty
{
};

struct Drawable
{
    std::uint32_t pickId = 0;
};

struct InstanceXform
{
    AcGeMatrix3d matrix;
};

struct LayerRef
{
    std::string name;
};

class SceneStore
{
public:
    // Creates (or returns) the ECS entity bound to |objectId|.
    entt::entity ensure(acdb::AcDbObjectId objectId)
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

    // The bound ECS entity or entt::null.
    entt::entity find(acdb::AcDbObjectId objectId) const
    {
        const auto found = byId_.find(objectId.persistentHandle().value);
        return found != byId_.end() ? found->second : entt::null;
    }

    bool contains(acdb::AcDbObjectId objectId) const
    {
        return byId_.count(objectId.persistentHandle().value) != 0;
    }

    // The objectId bound to |entity|, or kNullObjectId for entities
    // outside the mirror (entt::null included).
    acdb::AcDbObjectId objectIdOf(entt::entity entity) const
    {
        if (entity == entt::null || !registry_.has<SceneIdentity>(entity))
            return acdb::kNullObjectId;
        return registry_.get<SceneIdentity>(entity).objectId;
    }

    // Drops the ECS entity and every component (the AcDb record itself
    // is untouched — the store mirrors the document, never owns it).
    void forget(acdb::AcDbObjectId objectId)
    {
        const auto found = byId_.find(objectId.persistentHandle().value);
        if (found == byId_.end())
            return;
        registry_.destroy(found->second);
        byId_.erase(found);
        ++revision_;
    }

    void markDirty(acdb::AcDbObjectId objectId)
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

    bool isDirty(acdb::AcDbObjectId objectId) const
    {
        const entt::entity entity = find(objectId);
        return entity != entt::null && registry_.has<Dirty>(entity);
    }

    void clearDirty(acdb::AcDbObjectId objectId)
    {
        const entt::entity entity = find(objectId);
        if (entity != entt::null && registry_.has<Dirty>(entity))
            registry_.remove<Dirty>(entity);
    }

    // Visits every dirty entity; |fn|(AcDbObjectId).  Clearing dirtiness
    // during the walk is allowed (visited first, cleared after).
    template <typename Fn> void forEachDirty(Fn &&fn)
    {
        auto view = registry_.view<SceneIdentity, Dirty>();
        // Collect first: fn may mutate components (entt views iterate
        // live groups).
        pendingDirty_.clear();
        for (entt::entity entity : view)
            pendingDirty_.push_back(
                view.get<SceneIdentity>(entity).objectId);
        for (const acdb::AcDbObjectId objectId : pendingDirty_)
        {
            fn(objectId);
            clearDirty(objectId);
        }
    }

    std::size_t dirtyCount() const
    {
        return registry_.size<Dirty>();
    }

    std::size_t count() const { return byId_.size(); }

    template <typename Fn> void forEachObjectId(Fn &&fn) const
    {
        for (const auto &[handle, entity] : byId_)
        {
            (void)entity;
            fn(acdb::AcDbObjectId{acdb::AcDbHandle{handle}});
        }
    }

    // Monotonic mirror generation: bumped when the mirror state actually
    // changes (first ensure of an objectId, forget, a newly tagged Dirty).
    // Derived-data owners (draw-list caches, batch-sync systems) compare
    // revisions instead of polling entity details.
    std::uint64_t revision() const { return revision_; }

    entt::registry &registry() { return registry_; }
    const entt::registry &registry() const { return registry_; }

private:
    entt::registry registry_;
    // Keyed by the persistent handle value: the objectId is a wrapper,
    // the handle is its anchor, so the map key stays u64 end to end.
    std::unordered_map<std::uint64_t, entt::entity> byId_;
    std::vector<acdb::AcDbObjectId> pendingDirty_;
    std::uint64_t revision_ = 0;
};

} // namespace acgs
