#include "mid/animation.hpp"
#include "mid/script_runtime.hpp"
#include <cmath>
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 mid::animation::Skeleton skeleton; skeleton.joints={{"root",-1,{0,0,0},{0,0,0}},{"elbow",0,{1,0,0},{1,0,0}},{"hand",1,{2,0,0},{2,0,0}}};
 mid::animation::Clip clip;clip.name="move";clip.duration=1;clip.tracks={{2,{{0,{2,0,0}},{1,{1,1,0}}},{{0,{0,0,0}},{1,{0,90,0}}}}};
 mid::animation::Animator animator(skeleton);animator.play(clip,false);animator.update(.5F);
 check(std::abs(animator.skeleton().joints[2].position.x-1.5F)<.001F,"animation interpolates position keyframes");
 check(std::abs(animator.skeleton().joints[2].rotationDegrees.y-45.0F)<.01F,"animation interpolates rotation keyframes");
 mid::animation::Clip scaleClip;scaleClip.name="scale";scaleClip.duration=1;scaleClip.tracks={{2,{}, {}, {{0,{1,1,1}},{1,{3,2,1}}}}};animator.play(scaleClip,false);animator.update(.5F);check(std::abs(animator.skeleton().joints[2].scale.x-2.0F)<.001F&&std::abs(animator.skeleton().joints[2].scale.y-1.5F)<.001F,"animation interpolates scale keyframes");
 auto constrainedSkeleton=skeleton;auto constrainedIk=mid::animation::solveCcdConstrained(constrainedSkeleton,{0,1,2},{1,1,0},{{1,{-180,-180,0},{180,180,0},45.0F}},64,.02F);check(!constrainedIk.converged&&std::abs(constrainedSkeleton.joints[1].rotationDegrees.z)<.01F,"CCD IK respects a zero-degree joint rotation limit");
 auto ik=mid::animation::solveCcd(skeleton,{0,1,2},{1,1,0},64,.02F);
 check(ik.finalError<.1F,"CCD IK moves the end effector toward target");
 mid::Scene scene;auto entity=scene.createEntity("scripted");
 mid::script::Runtime runtime(4);mid::script::Program p{"move",{{mid::script::OpCode::SetPosition,entity,{3,2,1},true}}};
 auto result=runtime.execute(p,scene);check(result.completed&&scene.get(entity).transform.position.x==3,"script updates native scene through safe command VM");
 mid::script::Runtime limited(1);p.instructions.resize(2);check(!limited.execute(p,scene).completed,"instruction budget rejects oversized scripts");
 const auto before=scene.get(entity).transform.position.x;mid::script::Program invalid{"atomic",{{mid::script::OpCode::SetPosition,entity,{9,9,9},true},{mid::script::OpCode::SetPosition,999999,{0,0,0},true}}};check(!runtime.execute(invalid,scene).completed&&scene.get(entity).transform.position.x==before,"failed scripts roll back all scene mutations");
 mid::animation::Skeleton skinSkeleton;skinSkeleton.joints={{"root",-1,{0,0,0},{1,0,0},{0,0,0},{0,90,0}},{"child",0,{0,0,1},{0,0,1},{0,0,0},{0,0,0}}};auto worldPose=mid::animation::evaluateWorldPose(skinSkeleton);check(std::abs(worldPose.joints[1].position.x-2.0F)<0.01F,"hierarchical pose applies parent rotation to child offset");
 skinSkeleton.joints[0].scale={2,1,1};auto scaledPose=mid::animation::evaluateWorldPose(skinSkeleton);check(std::abs(scaledPose.joints[1].position.x-4.0F)<0.01F,"hierarchical pose applies parent scale to child offset");
 mid::animation::Pose poseA=worldPose,poseB=worldPose;poseB.joints[0].position.x=3.0F;auto blended=mid::animation::blendPoses(poseA,poseB,0.5F);check(std::abs(blended.joints[0].position.x-2.0F)<0.01F,"pose blending interpolates joint transforms");
 mid::render::Vertex bindVertex;bindVertex.position={0,0,1};bindVertex.normal={0,0,1};mid::animation::SkinInfluence influence;influence.joints={1,0,0,0};influence.weights={1,0,0,0};auto skinned=mid::animation::skinMesh(skinSkeleton,{bindVertex},{influence});check(std::abs(skinned[0].position.x-2.0F)<0.01F&&std::abs(skinned[0].position.z)<0.01F,"CPU skeletal skinning transforms vertices from bind pose to animated pose");
 if(failures)return 1;std::cout<<"Native animation/script tests passed\n";return 0;
}
