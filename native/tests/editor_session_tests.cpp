#include "mid/editor_session.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\\n'; ++failures; } };
    mid::Scene scene("Editor test");
    const auto id = scene.createEntity("Cube");
    mid::editor::EditorSession editor(scene);
    editor.translateAxis(id, mid::editor::Axis::X, 4.0);
    check(editor.editScene().get(id).transform.position.x == 4.0, "axis gizmo translates selected axis");
    editor.translatePlane(id, mid::editor::Axis::Y, 2.0, 3.0);
    check(editor.editScene().get(id).transform.position.x == 6.0 && editor.editScene().get(id).transform.position.z == 3.0, "plane gizmo translates two axes");
    check(editor.undo(), "undo is available");
    check(editor.editScene().get(id).transform.position.x == 4.0, "undo restores previous scene snapshot");
    check(editor.redo(), "redo is available");
    check(editor.editScene().get(id).transform.position.x == 6.0, "redo reapplies edit");
    editor.enterPlayMode();
    mid::Transform runtime = editor.runtimeScene()->get(id).transform;
    runtime.position.y = 99.0;
    editor.setRuntimeTransform(id, runtime);
    check(editor.runtimeScene()->get(id).transform.position.y == 99.0, "runtime scene accepts play-mode simulation updates");
    check(editor.editScene().get(id).transform.position.y == 2.0, "runtime mutations do not modify authoring scene");
    bool locked = false;
    try { editor.translateAxis(id, mid::editor::Axis::X, 1.0); } catch (const std::logic_error&) { locked = true; }
    check(locked, "authoring gizmos are locked during play mode");
    editor.stopPlayMode();
    check(!editor.playing(), "stop play mode discards runtime copy");
    return failures == 0 ? 0 : 1;
}
