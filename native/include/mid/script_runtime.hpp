#pragma once
#include "mid/scene.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace mid::script {
enum class OpCode { SetPosition, SetRotation, SetScale, SetActive, AddPosition, Halt };
struct Instruction { OpCode op{OpCode::Halt}; EntityId entity{0}; Vec3 value{}; bool flag{true}; };
struct Program { std::string name; std::vector<Instruction> instructions; };
struct ExecutionResult { bool completed{false}; std::size_t executed{0}; std::string error; };
// Data-only VM: scripts cannot call native functions, access files, or allocate unbounded memory.
class Runtime final {
public:
    explicit Runtime(std::size_t instructionBudget=10000);
    [[nodiscard]] ExecutionResult execute(const Program& program,Scene& scene) const noexcept;
    [[nodiscard]] std::size_t instructionBudget() const noexcept;
private: std::size_t budget_;
};
} // namespace mid::script
