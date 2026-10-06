#pragma once

// acgs::SceneStore (namespace acgs) — the ECS scene layer the roadmap
// assigns to attributes/scene/selection ("拓扑用紧凑 arena，属性/场景/
// 选择/撤销才上 ECS").  entt-backed; every ECS entity is keyed one-to-one
// by an AcDbHandle so the ECS never becomes a second ID space ("全链路
// ID 化禁传指针").  The brep topology layer keeps its arena and stays
// out of here.
//
// v1 components:
//   SceneIdentity  the owning AcDbHandle (carried on every ECS entity)
//   Drawable       render-side identity (objectId feeding the GPU ID
//                  buffer; equals the handle value today, may diverge)
//   Dirty          rebuild flag consumed by the batch-sync system
//   InstanceXform  world transform for block-reference instances
//   LayerRef       resolved layer name (attribute layer)
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
    acdb::AcDbHandle handle;
};

struct Dirty
{
};

struct Drawable
{
    std::uint32_t objectId = 0;
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
    // Creates (or returns) the ECS entity bound to |handle|.
    entt::entity ensure(acdb::AcDbHandle handle)
    {
        const auto key = handle.value;
        auto found = byHandle_.find(key);
        if (found != byHandle_.end())
            return found->second;
        const entt::entity entity = registry_.create();
        byHandle_.emplace(key, entity);
        registry_.assign<SceneIdentity>(entity, SceneIdentity{handle});
        return entity;
    }

    // The bound ECS entity or entt::null.
    entt::entity find(acdb::AcDbHandle handle) const
    {
        const auto found = byHandle_.find(handle.value);
        return found != byHandle_.end() ? found->second
                                        : entt::null;
    }

    bool contains(acdb::AcDbHandle handle) const
    {
        return byHandle_.count(handle.value) != 0;
    }

    // Drops the ECS entity and every component (the AcDb record itself
    // is untouched — the store mirrors the document, never owns it).
    void forget(acdb::AcDbHandle handle)
    {
        const auto found = byHandle_.find(handle.value);
        if (found == byHandle_.end())
            return;
        registry_.destroy(found->second);
        byHandle_.erase(found);
    }

    void markDirty(acdb::AcDbHandle handle)
    {
        const entt::entity entity = ensure(handle);
        if (registry_.has<Dirty>(entity))
            registry_.replace<Dirty>(entity);
        else
            registry_.assign<Dirty>(entity);
    }

    bool isDirty(acdb::AcDbHandle handle) const
    {
        const entt::entity entity = find(handle);
        return entity != entt::null && registry_.has<Dirty>(entity);
    }

    void clearDirty(acdb::AcDbHandle handle)
    {
        const entt::entity entity = find(handle);
        if (entity != entt::null)
            registry_.remove<Dirty>(entity);
    }

    // Visits every dirty entity; |fn|(AcDbHandle).  Clearing dirtiness
    // during the walk is allowed (visited first, cleared after).
    template <typename Fn> void forEachDirty(Fn &&fn)
    {
        auto view = registry_.view<SceneIdentity, Dirty>();
        // Collect first: fn may mutate components (entt views iterate
        // live groups).
        pendingDirty_.clear();
        for (entt::entity entity : view)
            pendingDirty_.push_back(
                view.get<SceneIdentity>(entity).handle);
        for (const acdb::AcDbHandle handle : pendingDirty_)
        {
            fn(handle);
            clearDirty(handle);
        }
    }

    std::size_t dirtyCount() const
    {
        return registry_.size<Dirty>();
    }

    std::size_t count() const { return byHandle_.size(); }

    entt::registry &registry() { return registry_; }
    const entt::registry &registry() const { return registry_; }

private:
    entt::registry registry_;
    std::unordered_map<std::uint64_t, entt::entity> byHandle_;
    std::vector<acdb::AcDbHandle> pendingDirty_;
};

} // namespace acgs
