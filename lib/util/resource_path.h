#pragma once

// Resource-path lookup for the bundled resources/ tree.  Tries an
// explicit GRID_RESOURCE_DIR override first, then walks the typical
// in-tree / install layouts, and finally falls back to the current
// working directory.  Returns the would-be path under the fallback so
// callers can detect a missing asset.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace util
{

namespace fs = std::filesystem;

// Resolve "resources/<rel>" against the search paths used by the demo
// and tests.  See docs/resources/README.md for the full layout and the
// environment override behavior.
inline std::filesystem::path resourcePath(const std::string &relative)
{
    // 1. Explicit override.
    if (const char *overrideDir = std::getenv("GRID_RESOURCE_DIR");
        overrideDir && *overrideDir)
    {
        const fs::path candidate =
            fs::path(overrideDir) / fs::path("resources") / relative;
        std::error_code ec;
        if (fs::exists(candidate, ec))
            return candidate;
    }

    // 2-4. Walk relative to the executable path.
    if (const char *exeRaw = std::getenv("GRID_EXECUTABLE_DIR");
        exeRaw && *exeRaw)
    {
        std::vector<fs::path> bases;
        bases.emplace_back(fs::path(exeRaw));
        bases.emplace_back(fs::path(exeRaw).parent_path());
        bases.emplace_back(fs::path(exeRaw).parent_path().parent_path());
        for (const fs::path &base : bases)
        {
            const fs::path candidate = base / "resources" / relative;
            std::error_code ec;
            if (fs::exists(candidate, ec))
                return candidate;
        }
    }

    // 5. Fall back to the current working directory.
    const fs::path cwdCandidate = fs::current_path() / "resources" / relative;
    if (fs::exists(cwdCandidate))
        return cwdCandidate;

    // Return the working-directory candidate even if missing; callers
    // can fs::exists() it for a precise error message.
    return cwdCandidate;
}

// Convenience: returns true when the resolved resource actually exists
// on disk.
inline bool resourceExists(const std::string &relative)
{
    std::error_code ec;
    return fs::exists(resourcePath(relative), ec);
}

} // namespace util
