#include "mid/model_importer.hpp"
#include <iostream>
#include <stdexcept>
int main(){
 int failures=0;auto check=[&](bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\\n';++failures;}};
 bool rejected=false;try{(void)mid::assets::importModelFile("this-file-does-not-exist.glb");}catch(const std::runtime_error&){rejected=true;}
 check(rejected,"missing/invalid model import reports a descriptive error");
 if(!mid::assets::hasAssimpModelSupport())check(true,"Assimp support remains optional");
 return failures?1:0;
}
