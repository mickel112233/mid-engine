#pragma once

#include "mid/scene.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace mid::editor {

enum class Axis { X, Y, Z };

// Owns an editable scene, snapshot-based undo/redo, constrained gizmo edits,
// and an isolated play-mode copy. Runtime mutations never leak into edit state.
class EditorSession final {
public:
    explicit EditorSession(Scene scene = Scene{});

    [[nodiscard]] const Scene& editScene() const noexcept;
    [[nodiscard]] const Scene* runtimeScene() const noexcept;
    [[nodiscard]] bool playing() const noexcept;
    [[nodiscard]] std::size_t undoCount() const noexcept;
    [[nodiscard]] std::size_t redoCount() const noexcept;

    void setTransform(EntityId entity, const Transform& transform);
    void translateAxis(EntityId entity, Axis axis, double amount);
    void translatePlane(EntityId entity, Axis normalAxis, double first, double second);
    bool undo();
    bool redo();

    void enterPlayMode();
    void stopPlayMode();
    // Runtime-only mutation used by simulation/script integration.
    void setRuntimeTransform(EntityId entity, const Transform& transform);

private:
    void recordBeforeMutation();
    Scene editScene_;
    std::optional<Scene> runtimeScene_;
    std::vector<Scene> history_;
    std::size_t cursor_{0};
};

} // namespace mid::editor
