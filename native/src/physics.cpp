#include "mid/physics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace mid::physics {
namespace {
float dot(Vec3f a,Vec3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3f sub(Vec3f a,Vec3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3f add(Vec3f a,Vec3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3f mul(Vec3f a,float s){return {a.x*s,a.y*s,a.z*s};}
float length(Vec3f a){return std::sqrt(dot(a,a));}
bool finite(Vec3f a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Cell { int x,y,z; bool operator==(const Cell&) const = default; };
struct CellHash { std::size_t operator()(const Cell& c) const noexcept { return std::hash<int>{}(c.x) ^ (std::hash<int>{}(c.y)<<1U) ^ (std::hash<int>{}(c.z)<<2U); };
}
PhysicsWorld::PhysicsWorld(Vec3f gravity,float fixedStep):gravity_(gravity),fixedStep_(fixedStep) {
    if(!finite(gravity)||!std::isfinite(fixedStep)||fixedStep<=0.0F) throw std::invalid_argument("Physics gravity and fixed step must be finite; step must be positive");
}
void PhysicsWorld::addBody(RigidBody bodyValue) {
    if(bodyValue.entityId==0||body(bodyValue.entityId)!=nullptr) throw std::invalid_argument("Rigid body entity ID must be unique and non-zero");
    if(!finite(bodyValue.position)||!finite(bodyValue.velocity)||!finite(bodyValue.force)||!std::isfinite(bodyValue.inverseMass)||
       !std::isfinite(bodyValue.restitution)||!std::isfinite(bodyValue.linearDamping)||!std::isfinite(bodyValue.radius)||
       bodyValue.inverseMass<0.0F||bodyValue.restitution<0.0F||bodyValue.restitution>1.0F||
       bodyValue.linearDamping<0.0F||bodyValue.radius<=0.0F) throw std::invalid_argument("Rigid body values are invalid");
    bodies_.push_back(bodyValue);
}
bool PhysicsWorld::removeBody(std::uint64_t id) {
    const auto it=std::find_if(bodies_.begin(),bodies_.end(),[&](const auto& b){return b.entityId==id;});
    if(it==bodies_.end())return false;
    bodies_.erase(it);return true;
}
RigidBody* PhysicsWorld::body(std::uint64_t id) noexcept {for(auto& b:bodies_)if(b.entityId==id)return &b;return nullptr;}
const RigidBody* PhysicsWorld::body(std::uint64_t id) const noexcept {for(const auto& b:bodies_)if(b.entityId==id)return &b;return nullptr;}
void PhysicsWorld::applyForce(std::uint64_t id,Vec3f force) {
    auto* b=body(id);if(!b)throw std::out_of_range("Rigid body does not exist");
    if(!finite(force))throw std::invalid_argument("Force must be finite");
    b->force=add(b->force,force);b->sleeping=false;
}
std::size_t PhysicsWorld::advance(float elapsed,std::size_t maxSubsteps) {
    if(!std::isfinite(elapsed)||elapsed<0.0F)throw std::invalid_argument("Elapsed time must be finite and non-negative");
    if(maxSubsteps==0)return 0;
    accumulator_+=std::min(elapsed,fixedStep_*static_cast<float>(maxSubsteps));
    std::size_t steps=0;
    while(accumulator_>=fixedStep_&&steps<maxSubsteps){step();accumulator_-=fixedStep_;++steps;}
    if(steps==maxSubsteps&&accumulator_>=fixedStep_)accumulator_=std::fmod(accumulator_,fixedStep_);
    return steps;
}
void PhysicsWorld::step() {
    contacts_.clear();
    stats_={}; stats_.bodies=bodies_.size();
    for(auto& b:bodies_) {
        if(b.kinematic||b.sleeping||b.inverseMass==0.0F){b.force={};continue;}
        const Vec3f acceleration=add(gravity_,mul(b.force,b.inverseMass));
        b.velocity=add(b.velocity,mul(acceleration,fixedStep_));
        b.velocity=mul(b.velocity,std::max(0.0F,1.0F-b.linearDamping*fixedStep_));
        b.position=add(b.position,mul(b.velocity,fixedStep_));
        b.force={};
    }
    std::unordered_map<Cell,std::vector<std::size_t>,CellHash> grid;
    constexpr float cellSize=2.0F;
    for(std::size_t i=0;i<bodies_.size();++i){
        const auto& b=bodies_[i];
        const int minX=static_cast<int>(std::floor((b.position.x-b.radius)/cellSize)),maxX=static_cast<int>(std::floor((b.position.x+b.radius)/cellSize));
        const int minY=static_cast<int>(std::floor((b.position.y-b.radius)/cellSize)),maxY=static_cast<int>(std::floor((b.position.y+b.radius)/cellSize));
        const int minZ=static_cast<int>(std::floor((b.position.z-b.radius)/cellSize)),maxZ=static_cast<int>(std::floor((b.position.z+b.radius)/cellSize));
        if(maxX-minX>64||maxY-minY>64||maxZ-minZ>64) throw std::runtime_error("Rigid body exceeds safe broadphase cell span");
        for(int x=minX;x<=maxX;++x)for(int y=minY;y<=maxY;++y)for(int z=minZ;z<=maxZ;++z)grid[{x,y,z}].push_back(i);
    }
    std::unordered_set<std::uint64_t> pairs;
    for(const auto& [cell,ids]:grid){(void)cell;for(std::size_t a=0;a<ids.size();++a)for(std::size_t b=a+1;b<ids.size();++b){
        const auto i=std::min(ids[a],ids[b]),j=std::max(ids[a],ids[b]);
        pairs.insert((static_cast<std::uint64_t>(i)<<32U)|static_cast<std::uint64_t>(j));
    }}
    stats_.broadphaseCandidates=pairs.size();
    for(const auto pair:pairs) {
        const auto i=static_cast<std::size_t>(pair>>32U),j=static_cast<std::size_t>(pair&0xffffffffULL);
        if(i>=bodies_.size()||j>=bodies_.size())continue;
        auto& a=bodies_[i];auto& b=bodies_[j];
        ++stats_.narrowphaseTests;
        if((a.kinematic||a.inverseMass==0.0F)&&(b.kinematic||b.inverseMass==0.0F))continue;
        const Vec3f delta=sub(b.position,a.position);const float distance=length(delta),sum=a.radius+b.radius;
        if(distance>=sum)continue;
        const Vec3f normal=distance>1.0e-6F?mul(delta,1.0F/distance):Vec3f{1,0,0};
        const float penetration=sum-distance;contacts_.push_back({a.entityId,b.entityId,normal,penetration});
        const float invA=(a.kinematic?0.0F:a.inverseMass),invB=(b.kinematic?0.0F:b.inverseMass),total=invA+invB;
        if(total<=0.0F)continue;
        const Vec3f correction=mul(normal,penetration/total*0.8F);
        a.position=sub(a.position,mul(correction,invA));b.position=add(b.position,mul(correction,invB));
        const Vec3f relative=sub(b.velocity,a.velocity);const float along=dot(relative,normal);
        if(along<0.0F){const float restitution=std::min(a.restitution,b.restitution);const float impulse=-(1.0F+restitution)*along/total;
            const Vec3f impulseVector=mul(normal,impulse);a.velocity=sub(a.velocity,mul(impulseVector,invA));b.velocity=add(b.velocity,mul(impulseVector,invB));}
    }
    stats_.contacts=contacts_.size();
}
const std::vector<Contact>& PhysicsWorld::contacts()const noexcept{return contacts_;}
float PhysicsWorld::interpolationAlpha()const noexcept{return accumulator_/fixedStep_;}
std::size_t PhysicsWorld::bodyCount()const noexcept{return bodies_.size();}
const PhysicsStats& PhysicsWorld::stats()const noexcept{return stats_;}
} // namespace mid::physics
