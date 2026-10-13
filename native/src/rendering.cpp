#include "mid/rendering.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace mid::render {
namespace {
float dot(const Vec3f& a, const Vec3f& b) noexcept { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3f add(const Vec3f& a, const Vec3f& b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3f sub(const Vec3f& a, const Vec3f& b) noexcept { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3f mul(const Vec3f& a, float s) noexcept { return {a.x*s,a.y*s,a.z*s}; }
Vec3f cross(const Vec3f& a, const Vec3f& b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
float length(const Vec3f& v) noexcept { return std::sqrt(dot(v,v)); }
Vec3f normalized(const Vec3f& v) {
    const float n = length(v);
    if (!(n > 1.0e-6F) || !std::isfinite(n)) throw std::invalid_argument("Camera basis vectors must be non-zero and finite");
    return mul(v, 1.0F/n);
}
Plane planeThrough(const Vec3f& normal, const Vec3f& point) {
    const Vec3f n = normalized(normal);
    return {n, -dot(n, point)};
}
bool finite(float v) noexcept { return std::isfinite(v); }
}

std::size_t MeshData::byteSize() const noexcept {
    return vertices.size()*sizeof(Vertex) + indices.size()*sizeof(std::uint32_t);
}
void MeshData::validate() const {
    if (vertices.empty()) throw std::invalid_argument("A mesh must contain vertices");
    if (!indices.empty() && indices.size()%3U != 0U) throw std::invalid_argument("Triangle index count must be divisible by three");
    for (const auto index : indices) if (index >= vertices.size()) throw std::invalid_argument("Mesh index is outside the vertex array");
    if (!finite(bounds.center.x) || !finite(bounds.center.y) || !finite(bounds.center.z) ||
        !finite(bounds.radius) || bounds.radius < 0.0F) throw std::invalid_argument("Mesh bounds must be finite with a non-negative radius");
    for (const auto& vertex : vertices) {
        if (!finite(vertex.position.x)||!finite(vertex.position.y)||!finite(vertex.position.z)||
            !finite(vertex.normal.x)||!finite(vertex.normal.y)||!finite(vertex.normal.z)||
            !finite(vertex.uv.x)||!finite(vertex.uv.y)||
            !finite(vertex.color.x)||!finite(vertex.color.y)||!finite(vertex.color.z)||!finite(vertex.color.w))
            throw std::invalid_argument("Mesh vertex data must be finite");
    }
}
void Material::validate() const {
    const float values[] = {baseColor.x,baseColor.y,baseColor.z,baseColor.w,metallic,roughness};
    for (const float value : values) if (!finite(value)) throw std::invalid_argument("Material values must be finite");
    if (metallic < 0.0F || metallic > 1.0F || roughness < 0.0F || roughness > 1.0F)
        throw std::invalid_argument("Metallic and roughness must be in [0, 1]");
}
void Camera::validate() const {
    const float values[] = {position.x,position.y,position.z,forward.x,forward.y,forward.z,up.x,up.y,up.z,
        verticalFovRadians,aspectRatio,nearPlane,farPlane};
    for (const float value : values) if (!finite(value)) throw std::invalid_argument("Camera values must be finite");
    if (verticalFovRadians <= 0.01F || verticalFovRadians >= 3.13F || aspectRatio <= 0.0F ||
        nearPlane <= 0.0F || farPlane <= nearPlane) throw std::invalid_argument("Camera projection parameters are invalid");
    (void)normalized(forward);
    (void)normalized(up);
}

bool Frustum::intersects(const SphereBounds& sphere) const noexcept {
    if (!finite(sphere.center.x)||!finite(sphere.center.y)||!finite(sphere.center.z)||
        !finite(sphere.radius)||sphere.radius < 0.0F) return false;
    for (const auto& plane : planes) {
        if (dot(plane.normal, sphere.center) + plane.distance < -sphere.radius) return false;
    }
    return true;
}
Frustum Frustum::fromCamera(const Camera& camera) {
    camera.validate();
    const Vec3f forward = normalized(camera.forward);
    const Vec3f right = normalized(cross(forward, camera.up));
    const Vec3f up = normalized(cross(right, forward));
    const float tanV = std::tan(camera.verticalFovRadians*0.5F);
    const float tanH = tanV*camera.aspectRatio;
    const Vec3f nearCenter = add(camera.position,mul(forward,camera.nearPlane));
    const Vec3f farCenter = add(camera.position,mul(forward,camera.farPlane));
    Frustum f;
    f.planes[0] = planeThrough(add(forward,mul(right,tanH)),camera.position);
    f.planes[1] = planeThrough(sub(forward,mul(right,tanH)),camera.position);
    f.planes[2] = planeThrough(add(forward,mul(up,tanV)),camera.position);
    f.planes[3] = planeThrough(sub(forward,mul(up,tanV)),camera.position);
    f.planes[4] = planeThrough(forward,nearCenter);
    f.planes[5] = planeThrough(mul(forward,-1.0F),farCenter);
    return f;
}

RenderPlan RenderPlanner::build(const Camera& camera, const std::vector<Renderable>& renderables,
                                const std::unordered_map<ResourceId, Material>& materials) const {
    camera.validate();
    RenderPlan plan;
    ScopedTimer timer(plan.stats.planningMilliseconds);
    const Frustum frustum = Frustum::fromCamera(camera);
    std::unordered_map<std::uint64_t, std::size_t> opaqueBatchIndex;
    std::unordered_map<std::uint64_t, std::size_t> transparentBatchIndex;
    for (const auto& item : renderables) {
        ++plan.stats.submitted;
        if (!item.active || item.mesh == 0 || item.material == 0) { ++plan.stats.culled; continue; }
        const auto materialIt = materials.find(item.material);
        if (materialIt == materials.end()) { ++plan.stats.culled; continue; }
        const Material& material = materialIt->second;
        if (!frustum.intersects(item.worldBounds)) { ++plan.stats.culled; continue; }
        ResourceId mesh = item.mesh;
        if (!item.lods.empty()) {
            const Vec3f delta = sub(item.worldBounds.center,camera.position);
            const float distance = length(delta);
            for (const auto& lod : item.lods) {
                if (distance <= lod.maxDistance) { if (lod.mesh != 0) mesh=lod.mesh; break; }
                if (lod.mesh != 0) mesh=lod.mesh;
            }
        }
        auto& batches = material.transparent ? plan.transparentBatches : plan.opaqueBatches;
        auto& lookup = material.transparent ? transparentBatchIndex : opaqueBatchIndex;
        // Mix IDs deterministically; collisions are resolved by verifying both IDs below.
        std::uint64_t key = mesh * 0x9E3779B185EBCA87ULL;
        key ^= materialIt->first + 0x9E3779B97F4A7C15ULL + (key<<6U) + (key>>2U);
        auto found = lookup.find(key);
        if (found == lookup.end() || batches[found->second].mesh != mesh || batches[found->second].material != item.material) {
            const auto existing = std::find_if(batches.begin(),batches.end(),[&](const DrawBatch& b){return b.mesh==mesh&&b.material==item.material;});
            if (existing != batches.end()) {
                found = lookup.emplace(key,static_cast<std::size_t>(std::distance(batches.begin(),existing))).first;
            } else {
                const auto index=batches.size();
                batches.push_back({mesh,item.material,{}});
                lookup[key]=index;
                found=lookup.find(key);
            }
        }
        batches[found->second].entityIds.push_back(item.entityId);
        ++plan.stats.visible;
    }
    plan.stats.batches=plan.opaqueBatches.size()+plan.transparentBatches.size();
    plan.stats.drawCalls=plan.stats.batches;
    plan.stats.estimatedInstanceCount=plan.stats.visible;
    return plan;
}

ResourceLifetime::ResourceLifetime(RenderDevice& device) : device_(&device) {}
ResourceLifetime::~ResourceLifetime() { shutdown(); }
ResourceHandle ResourceLifetime::create(ResourceKind kind,std::size_t bytes,std::string debugName) {
    if (stopped_ || device_==nullptr) throw std::logic_error("Resource lifetime manager is shut down");
    ResourceHandle handle=device_->createResource(kind,bytes,debugName);
    if (handle.id==0) throw std::runtime_error("Render device returned an invalid resource handle");
    if (!live_.emplace(handle.id,handle).second) {
        device_->destroyResource(handle);
        throw std::runtime_error("Render device returned a duplicate resource ID");
    }
    return handle;
}
void ResourceLifetime::retire(ResourceHandle handle,std::uint64_t afterFrame) {
    if (stopped_) return;
    const auto it=live_.find(handle.id);
    if (it==live_.end() || it->second.kind!=handle.kind) return;
    live_.erase(it);
    retired_.push_back({handle,afterFrame});
}
void ResourceLifetime::collect(std::uint64_t completedFrame) {
    if (stopped_ || device_==nullptr) return;
    auto it=retired_.begin();
    while (it!=retired_.end()) {
        if (it->frame<=completedFrame) {
            device_->destroyResource(it->handle);
            it=retired_.erase(it);
        } else ++it;
    }
}
void ResourceLifetime::shutdown() noexcept {
    if (stopped_) return;
    if (device_!=nullptr) {
        for (const auto& [id,handle] : live_) { (void)id; device_->destroyResource(handle); }
        for (const auto& retired : retired_) device_->destroyResource(retired.handle);
    }
    live_.clear(); retired_.clear(); stopped_=true; device_=nullptr;
}
std::size_t ResourceLifetime::liveCount() const noexcept { return live_.size(); }
std::size_t ResourceLifetime::pendingRetireCount() const noexcept { return retired_.size(); }

ScopedTimer::ScopedTimer(double& outputMilliseconds) noexcept
    : output_(&outputMilliseconds), start_(std::chrono::steady_clock::now()) {}
ScopedTimer::~ScopedTimer() {
    if (output_!=nullptr) *output_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start_).count();
}
} // namespace mid::render
