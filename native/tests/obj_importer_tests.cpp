#include "mid/obj_importer.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 const std::string obj="# square\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n";
 auto mesh=mid::assets::importObj(obj,"Square");check(mesh.vertices.size()==4,"OBJ vertices are deduplicated");check(mesh.indices.size()==6,"polygon is triangulated");check(mesh.bounds.radius>0.7F&&mesh.bounds.radius<0.72F,"mesh bounds are calculated");
 const auto negative=mid::assets::importObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");check(negative.indices.size()==3,"negative relative indices are supported");
 bool rejected=false;try{(void)mid::assets::importObj("v 0 0 0\nf 0 0 0\n");}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid zero indices are rejected");
 if(failures)return 1;std::cout<<"Native OBJ importer tests passed\n";return 0;
}
