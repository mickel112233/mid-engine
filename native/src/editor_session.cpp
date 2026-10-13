#include "mid/editor_session.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace mid::editor {
EditorSession::EditorSession(Scene scene) : editScene_(std::move(scene)) {
    history_.push_back(editScene_);
}
const Scene& EditorSession::editScene() const noexcept { return editScene_; }
const Scene* EditorSession::runtimeScene() const noexcept {
    return runtimeScene_ ? &*runtimeScene_ : nullptr;
}
bool EditorSession::playing() const noexcept { return runtimeScene_.has_value(); }
std::size_t EditorSession::undoCount() const noexcept { return cursor_; }
std::size_t EditorSession::redoCount() const noexcept {
    return history_.size() > cursor_ + 1 ? history_.size() - cursor_ - 1 : 0;
}
void EditorSession::recordBeforeMutation() {
    if (playing()) throw std::logic_error("Stop play mode before editing the authoring scene");
    if (cursor_ + 1 < history_.size()) history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1), history_.end());
    history_.push_back(editScene_);
    cursor_ = history_.size() - 1;
}
void EditorSession::setTransform(EntityId entity, const Transform& transform) {
    if (playing()) throw std::logic_error("Authoring transforms are locked during play mode");
    (void)editScene_.get(entity);
    Scene candidate = editScene_;
    candidate.setTransform(entity, transform);
    recordBeforeMutation();
    editScene_ = std::move(candidate);
    history_.back() = editScene_;
}
void EditorSession::translateAxis(EntityId entity, Axis axis, double amount) {
    if (!std::isfinite(amount)) throw std::invalid_argument("Gizmo delta must be finite");
    Transform t = editScene_.get(entity).transform;
    switch (axis) {
        case Axis::X: t.position.x += amount; break;
        case Axis::Y: t.position.y += amount; break;
        case Axis::Z: t.position.z += amount; break;
    }
    setTransform(entity, t);
}
void EditorSession::translatePlane(EntityId entity, Axis normalAxis, double first, double second) {
    if (!std::isfinite(first) || !std::isfinite(second)) throw std::invalid_argument("Gizmo deltas must be finite");
    Transform t = editScene_.get(entity).transform;
    switch (normalAxis) {
        case Axis::X: t.position.y += first; t.position.z += second; break;
        case Axis::Y: t.position.x += first; t.position.z += second; break;
        case Axis::Z: t.position.x += first; t.position.y += second; break;
    }
    setTransform(entity, t);
}
bool EditorSession::undo() {
    if (playing()) return false;
    if (cursor_ == 0) return false;
    --cursor_;
    editScene_ = history_[cursor_];
    return true;
}
bool EditorSession::redo() {
    if (playing() || cursor_ + 1 >= history_.size()) return false;
    ++cursor_;
    editScene_ = history_[cursor_];
    return true;
}
void EditorSession::enterPlayMode() {
    if (playing()) return;
    runtimeScene_ = editScene_;
}
void EditorSession::stopPlayMode() { runtimeScene_.reset(); }
void EditorSession::setRuntimeTransform(EntityId entity, const Transform& transform) {
    if (!runtimeScene_) throw std::logic_error("Play mode is not active");
    runtimeScene_->setTransform(entity, transform);
}
} // namespace mid::editor
