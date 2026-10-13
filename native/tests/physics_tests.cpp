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
    const auto y=world.body(1)->position.y;
    world.advance(1.0F/60.0F);
    check(world.body(1)->position.y<y,"gravity integrates velocity and position");
    check(world.interpolationAlpha()>=0&&world.interpolationAlpha()<1,"interpolation alpha remains normalized");
    bool rejected=false;try{world.addBody(a);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"duplicate body IDs are rejected");
    if(failures)return 1;std::cout<<"Native physics tests passed\n";return 0;
}
