#pragma once

// Arena (namespace brep) — bump allocator for the B-rep topology.  All
// topology objects live in arena chunks and are freed together when the
// arena resets; individual objects are never destroyed.  Pointers remain
// stable for the arena's lifetime (chunk reallocation never moves live
// objects), which the intrusive coedge links rely on.

#include <array>
#include <cstdint>
#include <vector>

namespace brep
{

class Arena
{
public:
    static constexpr std::size_t kChunkBytes = 64 * 1024;

    explicit Arena(std::size_t chunkBytes = kChunkBytes)
        : chunkBytes_(chunkBytes < 256 ? 256 : chunkBytes)
    {
    }

    Arena(const Arena &) = delete;
    Arena &operator=(const Arena &) = delete;

    template <typename T, typename... Args>
    T *create(Args &&...args)
    {
        void *memory = allocate(sizeof(T), alignof(T));
        return new (memory) T(static_cast<Args &&>(args)...);
    }

    void *allocate(std::size_t bytes, std::size_t alignment)
    {
        offset_ = (offset_ + alignment - 1) & ~(alignment - 1);
        if (chunks_.empty() || offset_ + bytes > chunkBytes_)
        {
            const std::size_t chunkSize =
                bytes > chunkBytes_ ? bytes : chunkBytes_;
            chunks_.emplace_back();
            chunks_.back().resize(chunkSize);
            offset_ = 0;
        }
        void *result = chunks_.back().data() + offset_;
        offset_ += bytes;
        return result;
    }

    // Frees everything at once; invalidates every pointer handed out.
    void reset()
    {
        chunks_.clear();
        offset_ = 0;
    }

    std::size_t chunkCount() const { return chunks_.size(); }
    std::size_t bytesReserved() const
    {
        return chunks_.size() * chunkBytes_;
    }

private:
    std::size_t chunkBytes_;
    std::size_t offset_ = 0;
    std::vector<std::vector<unsigned char>> chunks_;
};

} // namespace brep
