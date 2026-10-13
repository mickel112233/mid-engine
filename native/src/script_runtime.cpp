#include "mid/script_runtime.hpp"
#include <stdexcept>
namespace mid::script {
Runtime::Runtime(std::size_t budget):budget_(budget){if(budget==0)throw std::invalid_argument("Script instruction budget must be positive");}
ExecutionResult Runtime::execute(const Program& program,Scene& scene)const noexcept{
    ExecutionResult result;
    try{
        if(program.instructions.size()>budget_){result.error="Script exceeds instruction budget";return result;}
        for(const auto& instruction:program.instructions){
            ++result.executed;
            if(instruction.op==OpCode::Halt){result.completed=true;return result;}
            auto& entity=scene.get(instruction.entity);
            Transform transform=entity.transform;
            switch(instruction.op){
            case OpCode::SetPosition: transform.position=instruction.value;break;
            case OpCode::SetRotation: transform.rotationDegrees=instruction.value;break;
            case OpCode::SetScale: transform.scale=instruction.value;break;
            case OpCode::SetActive: entity.active=instruction.flag;continue;
            case OpCode::AddPosition:
                transform.position.x+=instruction.value.x;transform.position.y+=instruction.value.y;transform.position.z+=instruction.value.z;break;
            case OpCode::Halt: result.completed=true;return result;
            }
            scene.setTransform(instruction.entity,transform);
        }
        result.completed=true;
    }catch(const std::exception& e){result.error=e.what();}
    catch(...){result.error="Unknown script runtime error";}
    return result;
}
std::size_t Runtime::instructionBudget()const noexcept{return budget_;}
} // namespace mid::script
