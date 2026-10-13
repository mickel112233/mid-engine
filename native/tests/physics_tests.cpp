#include "mid/physics.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 mid::physics::PhysicsWorld world;
 mid::physics::RigidBody a;a.entityId=1;a.position={0,0,0};a.radius=1;
 mid::physics::RigidBody b;b.entityId=2;b.position={1,0,0};b.radius=1;b.inverseMass=0;
 world.addBody(a);world.addBody(b);
 check(world.advance(1.0F/60.0F)==1,"fixed-step advances once");
 check(!world.contacts().empty(),"overlapping spheres create a contact");
 check(world.body(1)->position.x<0,"dynamic body is separated from static body");
 const auto y=world.body(1)->position.y;world.advance(1.0F/60.0F);
 check(world.body(1)->position.y<y,"gravity integrates velocity and position");
 check(world.interpolationAlpha()>=0&&world.interpolationAlpha()<1,"interpolation alpha remains normalized");
 mid::physics::PhysicsWorld floorWorld({0,0,0});mid::physics::RigidBody falling;falling.entityId=9;falling.position={0,0.2F,0};falling.velocity={0,-1,0};falling.radius=0.5F;floorWorld.addBody(falling);floorWorld.addPlane({{0,1,0},0,0.2F});floorWorld.advance(1.0F/60.0F);
 check(floorWorld.body(9)->position.y>=0.49F,"sphere is corrected above static plane");
 check(!floorWorld.contacts().empty(),"plane collision creates contact");
 bool rejected=false;try{world.addBody(a);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"duplicate body IDs are rejected");
 check(world.stats().narrowphaseTests<=world.stats().broadphaseCandidates,"narrowphase tests are bounded by broadphase candidates");
 mid::physics::PhysicsWorld stress({0,0,0});for(std::uint64_t i=1;i<=200;++i){mid::physics::RigidBody body;body.entityId=i;body.position={static_cast<float>(i)*10.0F,0,0};stress.addBody(body);}stress.advance(1.0F/60.0F);check(stress.stats().broadphaseCandidates<200U*199U/2U,"spatial hash prunes sparse-scene collision candidates");
 mid::physics::PhysicsWorld boxWorld({0,0,0});mid::physics::RigidBody boxBody;boxBody.entityId=11;boxBody.position={0.2F,0.2F,0.2F};boxBody.radius=0.5F;boxWorld.addBody(boxBody);boxWorld.addBox({{{-1,-1,-1},{1,1,1}},0.0F});boxWorld.advance(1.0F/60.0F);check(!boxWorld.contacts().empty(),"sphere inside box collider creates a contact");
 mid::physics::PhysicsWorld jointWorld({0,0,0});mid::physics::RigidBody ja;ja.entityId=21;ja.position={0,0,0};ja.inverseMass=0;mid::physics::RigidBody jb;jb.entityId=22;jb.position={3,0,0};jb.inverseMass=1;jointWorld.addBody(ja);jointWorld.addBody(jb);jointWorld.addDistanceJoint({21,22,1.0F,30.0F,0.0F});jointWorld.advance(1.0F/60.0F);check(jointWorld.body(22)->position.x<3.0F,"spring distance joint pulls stretched body toward rest length"); if(failures)return 1;std::cout<<"Native physics tests passed\n";return 0;
}
