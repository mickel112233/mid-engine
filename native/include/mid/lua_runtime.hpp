#pragma once
#include "mid/scene.hpp"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace mid::script {
struct LuaConfig { std::size_t memoryLimitBytes{8U*1024U*1024U}; std::size_t instructionBudget{100000}; };
struct LuaResult { bool completed{false}; std::size_t instructions{0}; std::string error; std::vector<std::string> logs; std::optional<int> breakpointLine; };
class LuaScriptInstance final {
public:
 explicit LuaScriptInstance(std::string source,LuaConfig config={});
 ~LuaScriptInstance();
 LuaScriptInstance(const LuaScriptInstance&)=delete;
 LuaScriptInstance& operator=(const LuaScriptInstance&)=delete;
 LuaScriptInstance(LuaScriptInstance&&) noexcept;
 LuaScriptInstance& operator=(LuaScriptInstance&&) noexcept;
 [[nodiscard]] LuaResult start(Scene& scene);
 [[nodiscard]] LuaResult update(Scene& scene,float deltaSeconds);
 [[nodiscard]] LuaResult stop(Scene& scene);
 void setBreakpoints(std::vector<int> lines);
 [[nodiscard]] static bool available() noexcept;
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
 [[nodiscard]] LuaResult invoke(const char* callback,Scene& scene,float deltaSeconds,bool passDelta);
};
}
