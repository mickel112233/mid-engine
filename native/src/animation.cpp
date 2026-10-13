#include "mid/animation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mid::animation {
namespace {
Vec3f add(Vec3f a,Vec3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3f sub(Vec3f a,Vec3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3f mul(Vec3f a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(Vec3f a,Vec3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(Vec3f a){return std::sqrt(dot(a,a));}
Vec3f normalize(Vec3f v){const float n=length(v);return n>1e-7F?mul(v,1.0F/n):Vec3f{};}
Vec3f rotateAround(Vec3f v,Vec3f axis,float angle){
    axis=normalize(axis);const float c=std::cos(angle),s=std::sin(angle);
    return add(add(mul(v,c),mul(Vec3f{axis.y*v.z-axis.z*v.y,axis.z*v.x-axis.x*v.z,axis.x*v.y-axis.y*v.x},s)),mul(axis,dot(axis,v)*(1-c)));
}
Vec3f sample(const std::vector<Vec3Key>& keys,float t){
    if(keys.empty())return {};
    if(t<=keys.front().time)return keys.front().value;
    if(t>=keys.back().time)return keys.back().value;
    const auto upper=std::upper_bound(keys.begin(),keys.end(),t,[](float v,const Vec3Key& k){return v<k.time;});
    const auto& b=*upper;const auto& a=*(upper-1);const float span=b.time-a.time;
    const float alpha=span>0?(t-a.time)/span:0;
    return add(a.value,mul(sub(b.value,a.value),alpha));
}
}
void Skeleton::validate() const {
    for(std::size_t i=0;i<joints.size();++i){const auto& j=joints[i];if(j.parent>=static_cast<std::int32_t>(joints.size())||j.parent==static_cast<std::int32_t>(i)||j.parent< -1)throw std::invalid_argument("Skeleton joint parent index is invalid");
        if(!std::isfinite(j.bindPosition.x)||!std::isfinite(j.bindPosition.y)||!std::isfinite(j.bindPosition.z))throw std::invalid_argument("Skeleton joint position must be finite");
        std::int32_t p=j.parent;std::size_t hops=0;while(p>=0){if(++hops>joints.size())throw std::invalid_argument("Skeleton hierarchy contains a cycle");p=joints[static_cast<std::size_t>(p)].parent;}
    }
}
void Clip::validate(const Skeleton& skeleton) const {
    if(!std::isfinite(duration)||duration<0)throw std::invalid_argument("Animation duration is invalid");
    for(const auto& track:tracks){if(track.joint>=skeleton.joints.size())throw std::invalid_argument("Animation track joint is out of range");float previous=-1;
        for(const auto& key:track.positions){if(!std::isfinite(key.time)||key.time<0||key.time>duration||key.time<previous||!std::isfinite(key.value.x)||!std::isfinite(key.value.y)||!std::isfinite(key.value.z))throw std::invalid_argument("Animation keyframe is invalid or unsorted");previous=key.time;} previous=-1; for(const auto& key:track.rotations){if(!std::isfinite(key.time)||key.time<0||key.time>duration||key.time<previous||!std::isfinite(key.value.x)||!std::isfinite(key.value.y)||!std::isfinite(key.value.z))throw std::invalid_argument("Animation rotation keyframe is invalid or unsorted");previous=key.time;}}
}
Animator::Animator(Skeleton skeleton):skeleton_(std::move(skeleton)){skeleton_.validate();for(auto& j:skeleton_.joints){j.position=j.bindPosition;j.rotationDegrees=j.bindRotationDegrees;}}
void Animator::play(const Clip& clip,bool looping){clip.validate(skeleton_);clip_=clip;time_=0;looping_=looping;}
void Animator::stop()noexcept{clip_.reset();time_=0;for(auto& j:skeleton_.joints)j.position=j.bindPosition;}
void Animator::update(float delta){
    if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Animation delta must be finite and non-negative");
    if(!clip_)return;time_+=delta;
    if(clip_->duration>0){if(looping_)time_=std::fmod(time_,clip_->duration);else time_=std::min(time_,clip_->duration);}
    for(auto& j:skeleton_.joints){j.position=j.bindPosition;j.rotationDegrees=j.bindRotationDegrees;}
    for(const auto& track:clip_->tracks){if(!track.positions.empty())skeleton_.joints[track.joint].position=sample(track.positions,time_);if(!track.rotations.empty())skeleton_.joints[track.joint].rotationDegrees=sample(track.rotations,time_);}
}
const Skeleton& Animator::skeleton()const noexcept{return skeleton_;}
float Animator::time()const noexcept{return time_;}
IkResult solveCcd(Skeleton& skeleton,const std::vector<std::size_t>& chain,Vec3f target,std::size_t maxIterations,float tolerance){
    skeleton.validate();if(chain.size()<2)throw std::invalid_argument("IK chain requires at least two joints");
    if(!std::isfinite(target.x)||!std::isfinite(target.y)||!std::isfinite(target.z)||!std::isfinite(tolerance)||tolerance<0)throw std::invalid_argument("IK target/tolerance is invalid");
    for(auto id:chain)if(id>=skeleton.joints.size())throw std::out_of_range("IK chain joint index is invalid");
    for(std::size_t i=1;i<chain.size();++i)if(skeleton.joints[chain[i]].parent!=static_cast<std::int32_t>(chain[i-1]))throw std::invalid_argument("IK chain must follow direct parent links");
    auto& end=skeleton.joints[chain.back()].position;
    float error=length(sub(target,end));std::size_t iteration=0;
    for(;iteration<maxIterations&&error>tolerance;++iteration){
        for(std::size_t index=chain.size()-1;index-->1;){
            auto& pivot=skeleton.joints[chain[index]].position;
            Vec3f toEnd=sub(end,pivot),toTarget=sub(target,pivot);
            const float a=length(toEnd),b=length(toTarget);if(a<1e-7F||b<1e-7F)continue;
            const Vec3f axis=normalize(Vec3f{toEnd.y*toTarget.z-toEnd.z*toTarget.y,toEnd.z*toTarget.x-toEnd.x*toTarget.z,toEnd.x*toTarget.y-toEnd.y*toTarget.x});
            const float cosine=std::clamp(dot(toEnd,toTarget)/(a*b),-1.0F,1.0F);
            const float angle=std::acos(cosine);if(length(axis)<1e-7F||angle<1e-6F)continue;
            for(std::size_t child=index+1;child<chain.size();++child){auto& p=skeleton.joints[chain[child]].position;p=add(pivot,rotateAround(sub(p,pivot),axis,angle));}
        }
        error=length(sub(target,end));
    }
    return {error<=tolerance,iteration,error};
}
} // namespace mid::animation
