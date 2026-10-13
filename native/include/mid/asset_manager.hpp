#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mid::assets {

using AssetId = std::uint64_t;
enum class AssetState { Loading, Ready, Failed, Evicted };
struct AssetSnapshot {
    AssetId id{0};
    std::filesystem::path path;
    AssetState state{AssetState::Loading};
    std::size_t bytes{0};
    std::string error;
};

class AssetManager final {
public:
    using Loader = std::function<std::vector<std::byte>(const std::filesystem::path&)>;
    explicit AssetManager(std::size_t memoryBudgetBytes, Loader loader = {});
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    ~AssetManager();

    // Reuses a live cache entry for a normalized path. Loading occurs off-thread.
    [[nodiscard]] AssetId loadAsync(const std::filesystem::path& path);
    [[nodiscard]] std::optional<std::vector<std::byte>> data(AssetId id) const;
    [[nodiscard]] std::optional<AssetSnapshot> inspect(AssetId id) const;
    void wait(AssetId id);
    void setMemoryBudget(std::size_t bytes);
    [[nodiscard]] std::size_t memoryBudget() const noexcept;
    [[nodiscard]] std::size_t residentBytes() const noexcept;
    [[nodiscard]] std::vector<AssetSnapshot> snapshot() const;
    bool evict(AssetId id);
    void clear();

private:
    struct Entry {
        AssetSnapshot snapshot;
        std::vector<std::byte> data;
        std::shared_future<void> task;
        std::uint64_t lastUse{0};
    };
    static std::vector<std::byte> readFile(const std::filesystem::path& path);
    void finishLoad(AssetId id, std::vector<std::byte> bytes, std::string error);
    void enforceBudgetLocked();

    mutable std::mutex mutex_;
    std::unordered_map<AssetId, Entry> entries_;
    std::unordered_map<std::string, AssetId> byPath_;
    std::size_t budget_;
    std::size_t resident_{0};
    AssetId nextId_{1};
    std::uint64_t clock_{0};
    Loader loader_;
};

} // namespace mid::assets
