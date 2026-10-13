#include "mid/asset_manager.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mid::assets {
AssetManager::AssetManager(std::size_t memoryBudgetBytes, Loader loader)
    : budget_(memoryBudgetBytes), loader_(loader ? std::move(loader) : Loader(readFile)) {}
AssetManager::~AssetManager() { clear(); }

std::vector<std::byte> AssetManager::readFile(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input) throw std::runtime_error("Unable to open asset: "+path.string());
    const auto end=input.tellg();
    if (end<0) throw std::runtime_error("Unable to determine asset size: "+path.string());
    const auto size=static_cast<std::uintmax_t>(end);
    if (size>static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        throw std::runtime_error("Asset is too large for this process");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0,std::ios::beg);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Unable to read complete asset: "+path.string());
    return bytes;
}
AssetId AssetManager::loadAsync(const std::filesystem::path& inputPath) {
    const auto path=std::filesystem::absolute(inputPath).lexically_normal();
    const auto key=path.generic_string();
    std::lock_guard lock(mutex_);
    if (const auto found=byPath_.find(key); found!=byPath_.end()) {
        auto it=entries_.find(found->second);
        if (it!=entries_.end() && it->second.snapshot.state!=AssetState::Evicted) {
            it->second.lastUse=++clock_;
            return it->first;
        }
    }
    if (nextId_==0 || nextId_==std::numeric_limits<AssetId>::max()) throw std::overflow_error("Asset ID space exhausted");
    const AssetId id=nextId_++;
    Entry entry;
    entry.snapshot={id,path,AssetState::Loading,0,{}};
    entry.lastUse=++clock_;
    entries_.emplace(id,std::move(entry));
    byPath_[key]=id;
    auto it=entries_.find(id);
    it->second.task=std::async(std::launch::async,[this,id,path,loader=loader_]() mutable {
        try { finishLoad(id,loader(path),{}); }
        catch (const std::exception& e) { finishLoad(id,{},e.what()); }
        catch (...) { finishLoad(id,{},"Unknown asset loading error"); }
    });
    return id;
}
void AssetManager::finishLoad(AssetId id,std::vector<std::byte> bytes,std::string error) {
    std::lock_guard lock(mutex_);
    auto it=entries_.find(id);
    if (it==entries_.end()) return;
    Entry& entry=it->second;
    if (!error.empty()) {
        entry.snapshot.state=AssetState::Failed;
        entry.snapshot.error=std::move(error);
        return;
    }
    entry.snapshot.bytes=bytes.size();
    entry.snapshot.state=AssetState::Ready;
    entry.data=std::move(bytes);
    resident_+=entry.data.size();
    entry.lastUse=++clock_;
    enforceBudgetLocked();
}
std::optional<std::vector<std::byte>> AssetManager::data(AssetId id) const {
    std::lock_guard lock(mutex_);
    const auto it=entries_.find(id);
    if (it==entries_.end() || it->second.snapshot.state!=AssetState::Ready) return std::nullopt;
    return it->second.data;
}
std::optional<AssetSnapshot> AssetManager::inspect(AssetId id) const {
    std::lock_guard lock(mutex_);
    const auto it=entries_.find(id);
    if (it==entries_.end()) return std::nullopt;
    return it->second.snapshot;
}
void AssetManager::wait(AssetId id) {
    std::future<void>* task=nullptr;
    {
        std::lock_guard lock(mutex_);
        const auto it=entries_.find(id);
        if (it==entries_.end()) throw std::out_of_range("Unknown asset ID");
        task=&it->second.task;
    }
    if (task->valid()) task->wait();
}
void AssetManager::setMemoryBudget(std::size_t bytes) {
    std::lock_guard lock(mutex_); budget_=bytes; enforceBudgetLocked();
}
std::size_t AssetManager::memoryBudget() const noexcept { std::lock_guard lock(mutex_); return budget_; }
std::size_t AssetManager::residentBytes() const noexcept { std::lock_guard lock(mutex_); return resident_; }
std::vector<AssetSnapshot> AssetManager::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<AssetSnapshot> out; out.reserve(entries_.size());
    for (const auto& [id,entry] : entries_) { (void)id; out.push_back(entry.snapshot); }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    return out;
}
bool AssetManager::evict(AssetId id) {
    std::lock_guard lock(mutex_);
    const auto it=entries_.find(id);
    if (it==entries_.end() || it->second.snapshot.state!=AssetState::Ready) return false;
    resident_-=it->second.data.size();
    it->second.data.clear(); it->second.data.shrink_to_fit();
    it->second.snapshot.state=AssetState::Evicted;
    it->second.snapshot.bytes=0;
    return true;
}
void AssetManager::enforceBudgetLocked() {
    while (resident_>budget_) {
        auto victim=entries_.end();
        for (auto it=entries_.begin();it!=entries_.end();++it) {
            if (it->second.snapshot.state!=AssetState::Ready) continue;
            if (victim==entries_.end() || it->second.lastUse<victim->second.lastUse) victim=it;
        }
        if (victim==entries_.end()) break;
        resident_-=victim->second.data.size();
        victim->second.data.clear(); victim->second.data.shrink_to_fit();
        victim->second.snapshot.state=AssetState::Evicted;
        victim->second.snapshot.bytes=0;
    }
}
void AssetManager::clear() {
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard lock(mutex_);
        for (auto& [id,entry] : entries_) { (void)id; if (entry.task.valid()) tasks.push_back(std::move(entry.task)); }
    }
    for (auto& task : tasks) if (task.valid()) task.wait();
    std::lock_guard lock(mutex_);
    entries_.clear(); byPath_.clear(); resident_=0;
}
} // namespace mid::assets
