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
 if(failures)return 1;std::cout<<"Native physics tests passed\n";return 0;
}
