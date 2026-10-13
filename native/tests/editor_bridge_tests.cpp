#include "mid/editor_bridge.hpp"
#include <cmath>
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 mid::Scene scene("Editor scene");const auto root=scene.createEntity("Root");const auto child=scene.createEntity("Mesh");
 mid::Transform rt;rt.position={10,0,0};rt.rotationDegrees={0,90,0};scene.setTransform(root,rt);
 mid::Transform ct;ct.position={0,0,-5};scene.setTransform(child,ct);scene.setParent(child,root);
 mid::editor::SceneRenderBridge bridge;bridge.setRenderComponents(child,{4,7,{{1,0,0},.5F},true,{}});
 const auto snapshot=bridge.build(scene,{},{});check(snapshot.renderables.size()==1,"bridge exports renderable entities");
 const auto matrix=bridge.modelMatrix(scene,child);check(std::abs(matrix[12]-5.0F)<.01F&&std::abs(matrix[14])<.01F,"native renderer receives hierarchical world transform matrix");
 if(!snapshot.renderables.empty()){
   const auto p=snapshot.renderables[0].worldBounds.center;
   check(std::abs(p.x-5.0F)<.01F&&std::abs(p.z+1.0F)<.01F,"bridge resolves parent rotation, translation, and local bounds");
 }
 scene.get(child).active=false;check(bridge.build(scene,{},{}).renderables.empty(),"inactive entities are omitted from render snapshot");
 check(bridge.removeRenderComponents(child),"render component can be removed");
 if(failures)return 1;std::cout<<"Native editor bridge tests passed\n";return 0;
}
