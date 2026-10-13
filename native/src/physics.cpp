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
void PhysicsWorld::addPlane(PlaneCollider plane) {
    if(!finite(plane.normal)||!std::isfinite(plane.offset)||!std::isfinite(plane.restitution)||plane.restitution<0||plane.restitution>1) throw std::invalid_argument("Plane collider values are invalid");
    const float n=length(plane.normal); if(n<1e-7F) throw std::invalid_argument("Plane normal cannot be zero");
    plane.normal=mul(plane.normal,1.0F/n); plane.offset/=n; planes_.push_back(plane);
}
void PhysicsWorld::clearPlanes() noexcept { planes_.clear(); }
void PhysicsWorld::addBox(BoxCollider box) {
    const auto& a=box.bounds;
    if(!finite(a.minimum)||!finite(a.maximum)||!std::isfinite(box.restitution)||box.restitution<0.0F||box.restitution>1.0F||
       a.minimum.x>a.maximum.x||a.minimum.y>a.maximum.y||a.minimum.z>a.maximum.z)
        throw std::invalid_argument("Box collider bounds or restitution are invalid");
    boxes_.push_back(box);
}
void PhysicsWorld::clearBoxes() noexcept { boxes_.clear(); }
void PhysicsWorld::addDistanceJoint(DistanceJoint joint) {
    if(joint.a==0||joint.b==0||joint.a==joint.b||!body(joint.a)||!body(joint.b)||
       !std::isfinite(joint.restLength)||joint.restLength<0.0F||!std::isfinite(joint.stiffness)||joint.stiffness<0.0F||
       !std::isfinite(joint.damping)||joint.damping<0.0F)
        throw std::invalid_argument("Distance joint requires two existing bodies and finite non-negative parameters");
    joints_.push_back(joint);
}
void PhysicsWorld::clearDistanceJoints() noexcept { joints_.clear(); }
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
    // Spring/damper constraints contribute forces before fixed-step integration.
    for(const auto& joint:joints_) {
        auto* a=body(joint.a); auto* b=body(joint.b);
        if(!a||!b) continue;
        const Vec3f delta=sub(b->position,a->position); const float distance=length(delta);
        if(distance<1.0e-6F) continue;
        const Vec3f direction=mul(delta,1.0F/distance);
        const float relativeSpeed=dot(sub(b->velocity,a->velocity),direction);
        const float magnitude=joint.stiffness*(distance-joint.restLength)+joint.damping*relativeSpeed;
        const Vec3f force=mul(direction,magnitude);
        if(!a->kinematic&&a->inverseMass>0.0F) a->force=add(a->force,force);
        if(!b->kinematic&&b->inverseMass>0.0F) b->force=sub(b->force,force);
    }
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
    for(auto& body:bodies_) if(!body.kinematic&&body.inverseMass>0.0F) for(const auto& plane:planes_) {
        const float distance=dot(plane.normal,body.position)-plane.offset;
        if(distance>=body.radius) continue;
        const float penetration=body.radius-distance; body.position=add(body.position,mul(plane.normal,penetration));
        const float normalVelocity=dot(body.velocity,plane.normal);
        if(normalVelocity<0.0F) body.velocity=sub(body.velocity,mul(plane.normal,(1.0F+plane.restitution)*normalVelocity));
        contacts_.push_back({body.entityId,0,plane.normal,penetration});
    }
    for(auto& body:bodies_) if(!body.kinematic&&body.inverseMass>0.0F) for(const auto& box:boxes_) {
        const auto& lo=box.bounds.minimum; const auto& hi=box.bounds.maximum;
        const Vec3f closest{std::clamp(body.position.x,lo.x,hi.x),std::clamp(body.position.y,lo.y,hi.y),std::clamp(body.position.z,lo.z,hi.z)};
        Vec3f delta=sub(body.position,closest); float distance=length(delta);
        if(distance>=body.radius) continue;
        Vec3f normal{}; float penetration{};
        if(distance>1.0e-6F) { normal=mul(delta,1.0F/distance); penetration=body.radius-distance; }
        else {
            const float dx=std::min(body.position.x-lo.x,hi.x-body.position.x);
            const float dy=std::min(body.position.y-lo.y,hi.y-body.position.y);
            const float dz=std::min(body.position.z-lo.z,hi.z-body.position.z);
            if(dx<=dy&&dx<=dz) normal={body.position.x-(lo.x+hi.x)*0.5F<0?-1.0F:1.0F,0,0};
            else if(dy<=dz) normal={0,body.position.y-(lo.y+hi.y)*0.5F<0?-1.0F:1.0F,0};
            else normal={0,0,body.position.z-(lo.z+hi.z)*0.5F<0?-1.0F:1.0F};
            penetration=body.radius+std::min({dx,dy,dz});
        }
        body.position=add(body.position,mul(normal,penetration));
        const float vn=dot(body.velocity,normal);
        if(vn<0.0F) body.velocity=sub(body.velocity,mul(normal,(1.0F+std::min(body.restitution,box.restitution))*vn));
        contacts_.push_back({body.entityId,0,normal,penetration});
    }
    stats_.contacts=contacts_.size();
}
const std::vector<Contact>& PhysicsWorld::contacts()const noexcept{return contacts_;}
float PhysicsWorld::interpolationAlpha()const noexcept{return accumulator_/fixedStep_;}
std::size_t PhysicsWorld::bodyCount()const noexcept{return bodies_.size();}
const PhysicsStats& PhysicsWorld::stats()const noexcept{return stats_;}
} // namespace mid::physics
