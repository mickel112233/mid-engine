#pragma once

#include "mid/scene.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mid::render {

using ResourceId = std::uint64_t;

struct Vec2 { float x{0.0F}; float y{0.0F}; };
struct Vec3f { float x{0.0F}; float y{0.0F}; float z{0.0F}; };
struct Vec4f { float x{1.0F}; float y{1.0F}; float z{1.0F}; float w{1.0F}; };

struct Vertex {
    Vec3f position{};
    Vec3f normal{0.0F, 1.0F, 0.0F};
    Vec2 uv{};
    Vec4f color{};
};

struct SphereBounds {
    Vec3f center{};
    float radius{1.0F};
};

struct MeshData {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    SphereBounds bounds{};
    [[nodiscard]] std::size_t byteSize() const noexcept;
    void validate() const;
};

struct Material {
    std::string name;
    Vec4f baseColor{};
    float metallic{0.0F};
    float roughness{0.7F};
    bool transparent{false};
    bool doubleSided{false};
    void validate() const;
};

struct Camera {
    Vec3f position{0.0F, 0.0F, 5.0F};
    Vec3f forward{0.0F, 0.0F, -1.0F};
    Vec3f up{0.0F, 1.0F, 0.0F};
    float verticalFovRadians{1.04719755F};
    float aspectRatio{16.0F / 9.0F};
    float nearPlane{0.1F};
    float farPlane{1000.0F};
    void validate() const;
};

struct DirectionalLight {
    Vec3f direction{-0.4F, -1.0F, -0.3F};
    Vec3f color{1.0F, 0.98F, 0.92F};
    float intensity{1.0F};
};

struct Plane { Vec3f normal{}; float distance{0.0F}; };
struct Frustum {
    std::array<Plane, 6> planes{};
    [[nodiscard]] bool intersects(const SphereBounds& sphere) const noexcept;
    // Builds a conservative axis-aligned frustum around a camera forward vector.
    [[nodiscard]] static Frustum fromCamera(const Camera& camera);
};

struct LodLevel {
    float maxDistance{100.0F};
    ResourceId mesh{0};
};
struct Renderable {
    std::uint64_t entityId{0};
    ResourceId mesh{0};
    ResourceId material{0};
    SphereBounds worldBounds{};
    bool active{true};
    std::vector<LodLevel> lods;
};
struct DrawBatch {
    ResourceId mesh{0};
    ResourceId material{0};
    std::vector<std::uint64_t> entityIds;
};
struct RenderStats {
    std::size_t submitted{0};
    std::size_t culled{0};
    std::size_t visible{0};
    std::size_t drawCalls{0};
    std::size_t batches{0};
    std::size_t estimatedInstanceCount{0};
    double planningMilliseconds{0.0};
    double gpuMilliseconds{0.0};
    bool gpuTimingAvailable{false};
};
struct RenderPlan {
    std::vector<DrawBatch> opaqueBatches;
    std::vector<DrawBatch> transparentBatches;
    RenderStats stats;
};

class RenderPlanner final {
public:
    [[nodiscard]] RenderPlan build(const Camera& camera,
                                   const std::vector<Renderable>& renderables,
                                   const std::unordered_map<ResourceId, Material>& materials) const;
};

enum class ResourceKind { Buffer, Texture, Shader, Pipeline, Sampler, Other };
struct ResourceHandle {
    ResourceId id{0};
    ResourceKind kind{ResourceKind::Other};
    friend bool operator==(const ResourceHandle&, const ResourceHandle&) = default;
};

// Backend implementations must make destroy() safe for a resource that has already been lost.
class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    [[nodiscard]] virtual ResourceHandle createResource(ResourceKind kind, std::size_t bytes,
                                                         const std::string& debugName) = 0;
    virtual void destroyResource(ResourceHandle handle) noexcept = 0;
    virtual void beginFrame() = 0;
    virtual void submit(const RenderPlan& plan, const Camera& camera,
                        const DirectionalLight& light) = 0;
    virtual void endFrame() = 0;
};

class ResourceLifetime final {
public:
    explicit ResourceLifetime(RenderDevice& device);
    ResourceLifetime(const ResourceLifetime&) = delete;
    ResourceLifetime& operator=(const ResourceLifetime&) = delete;
    ~ResourceLifetime();

    [[nodiscard]] ResourceHandle create(ResourceKind kind, std::size_t bytes,
                                        std::string debugName);
    void retire(ResourceHandle handle, std::uint64_t afterFrame);
    void collect(std::uint64_t completedFrame);
    void shutdown() noexcept;
    [[nodiscard]] std::size_t liveCount() const noexcept;
    [[nodiscard]] std::size_t pendingRetireCount() const noexcept;
private:
    struct Retired { ResourceHandle handle; std::uint64_t frame; };
    RenderDevice* device_;
    std::unordered_map<ResourceId, ResourceHandle> live_;
    std::vector<Retired> retired_;
    bool stopped_{false};
};

class ScopedTimer final {
public:
    explicit ScopedTimer(double& outputMilliseconds) noexcept;
    ~ScopedTimer();
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
private:
    double* output_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace mid::render
