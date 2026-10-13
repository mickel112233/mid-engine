#include "mid/asset_manager.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
int failures=0;
void check(bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';++failures;}}
}
int main(){
    using namespace mid::assets;
    AssetManager manager(6,[](const std::filesystem::path& path){
        const auto name=path.filename().string();
        if(name=="missing.bin") throw std::runtime_error("fixture missing");
        return std::vector<std::byte>(name=="first.bin"?4U:5U,std::byte{0x2A});
    });
    const auto first=manager.loadAsync("virtual/first.bin");
    manager.wait(first);
    auto a=manager.inspect(first);
    check(a && a->state==AssetState::Ready && a->bytes==4,"async asset reaches ready state");
    const auto same=manager.loadAsync("virtual/first.bin");
    check(first==same,"normalized path is deduplicated");
    const auto second=manager.loadAsync("virtual/second.bin");
    manager.wait(second);
    check(manager.residentBytes()<=manager.memoryBudget(),"resident memory stays within budget after load");
    const auto secondState=manager.inspect(second);
    check(secondState && (secondState->state==AssetState::Ready || secondState->state==AssetState::Evicted),"asset has a defined budget outcome");
    check(manager.data(first).has_value() || manager.inspect(first)->state==AssetState::Evicted,"cache data is readable or explicitly evicted");
    const auto missing=manager.loadAsync("virtual/missing.bin");
    manager.wait(missing);
    const auto failed=manager.inspect(missing);
    check(failed && failed->state==AssetState::Failed && !failed->error.empty(),"load failures are surfaced");
    manager.setMemoryBudget(0);
    check(manager.residentBytes()==0,"zero budget evicts all resident assets");
    if(failures){std::cerr<<failures<<" asset manager test(s) failed\n";return 1;}
    std::cout<<"Native asset manager tests passed\n";return 0;
}
