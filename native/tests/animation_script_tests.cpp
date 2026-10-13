#include "mid/animation.hpp"
#include "mid/script_runtime.hpp"
#include <cmath>
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'
';++failures;}};
 mid::animation::Skeleton skeleton; skeleton.joints={{"root",-1,{0,0,0},{0,0,0}},{"elbow",0,{1,0,0},{1,0,0}},{"hand",1,{2,0,0},{2,0,0}}};
 mid::animation::Clip clip;clip.name="move";clip.duration=1;clip.tracks={{2,{{0,{2,0,0}},{1,{1,1,0}}},{{0,{0,0,0}},{1,{0,90,0}}}}};
 mid::animation::Animator animator(skeleton);animator.play(clip,false);animator.update(.5F);
 check(std::abs(animator.skeleton().joints[2].position.x-1.5F)<.001F,"animation interpolates position keyframes");
 check(std::abs(animator.skeleton().joints[2].rotationDegrees.y-45.0F)<.01F,"animation interpolates rotation keyframes");
 auto ik=mid::animation::solveCcd(skeleton,{0,1,2},{1,1,0},64,.02F);
 check(ik.finalError<.1F,"CCD IK moves the end effector toward target");
 mid::Scene scene;auto entity=scene.createEntity("scripted");
 mid::script::Runtime runtime(4);mid::script::Program p{"move",{{mid::script::OpCode::SetPosition,entity,{3,2,1},true}}};
 auto result=runtime.execute(p,scene);check(result.completed&&scene.get(entity).transform.position.x==3,"script updates native scene through safe command VM");
 mid::script::Runtime limited(1);p.instructions.resize(2);check(!limited.execute(p,scene).completed,"instruction budget rejects oversized scripts");
 const auto before=scene.get(entity).transform.position.x;mid::script::Program invalid{"atomic",{{mid::script::OpCode::SetPosition,entity,{9,9,9},true},{mid::script::OpCode::SetPosition,999999,{0,0,0},true}}};check(!runtime.execute(invalid,scene).completed&&scene.get(entity).transform.position.x==before,"failed scripts roll back all scene mutations");
 if(failures)return 1;std::cout<<"Native animation/script tests passed
";return 0;
}
