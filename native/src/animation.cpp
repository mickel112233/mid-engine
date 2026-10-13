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
        if(!std::isfinite(j.bindPosition.x)||!std::isfinite(j.bindPosition.y)||!std::isfinite(j.bindPosition.z)||!std::isfinite(j.bindRotationDegrees.x)||!std::isfinite(j.bindRotationDegrees.y)||!std::isfinite(j.bindRotationDegrees.z)||!std::isfinite(j.position.x)||!std::isfinite(j.position.y)||!std::isfinite(j.position.z)||!std::isfinite(j.rotationDegrees.x)||!std::isfinite(j.rotationDegrees.y)||!std::isfinite(j.rotationDegrees.z)||!std::isfinite(j.bindScale.x)||!std::isfinite(j.bindScale.y)||!std::isfinite(j.bindScale.z)||!std::isfinite(j.scale.x)||!std::isfinite(j.scale.y)||!std::isfinite(j.scale.z))throw std::invalid_argument("Skeleton bind/current pose must be finite");
        std::int32_t p=j.parent;std::size_t hops=0;while(p>=0){if(++hops>joints.size())throw std::invalid_argument("Skeleton hierarchy contains a cycle");p=joints[static_cast<std::size_t>(p)].parent;}
    }
}
void Clip::validate(const Skeleton& skeleton) const {
    if(!std::isfinite(duration)||duration<0)throw std::invalid_argument("Animation duration is invalid");
    for(const auto& track:tracks){if(track.joint>=skeleton.joints.size())throw std::invalid_argument("Animation track joint is out of range");float previous=-1;
        for(const auto& key:track.positions){if(!std::isfinite(key.time)||key.time<0||key.time>duration||key.time<previous||!std::isfinite(key.value.x)||!std::isfinite(key.value.y)||!std::isfinite(key.value.z))throw std::invalid_argument("Animation keyframe is invalid or unsorted");previous=key.time;} previous=-1; for(const auto& key:track.rotations){if(!std::isfinite(key.time)||key.time<0||key.time>duration||key.time<previous||!std::isfinite(key.value.x)||!std::isfinite(key.value.y)||!std::isfinite(key.value.z))throw std::invalid_argument("Animation rotation keyframe is invalid or unsorted");previous=key.time;} previous=-1; for(const auto& key:track.scales){if(!std::isfinite(key.time)||key.time<0||key.time>duration||key.time<previous||!std::isfinite(key.value.x)||!std::isfinite(key.value.y)||!std::isfinite(key.value.z))throw std::invalid_argument("Animation scale keyframe is invalid or unsorted");previous=key.time;}}
}
namespace {
struct Quat { float x{0},y{0},z{0},w{1}; };
Quat qmul(Quat a,Quat b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Quat qnorm(Quat q){const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return n>1e-7F?Quat{q.x/n,q.y/n,q.z/n,q.w/n}:Quat{};}
Quat qconj(Quat q){return {-q.x,-q.y,-q.z,q.w};}
Quat qfromEuler(Vec3f d){constexpr float k=0.00872664625997F;const float x=d.x*k,y=d.y*k,z=d.z*k;const Quat qx{std::sin(x),0,0,std::cos(x)},qy{0,std::sin(y),0,std::cos(y)},qz{0,0,std::sin(z),std::cos(z)};return qnorm(qmul(qmul(qz,qy),qx));}
Vec3f qrotate(Quat q,Vec3f v){const Quat p{v.x,v.y,v.z,0};const Quat r=qmul(qmul(q,p),qconj(q));return {r.x,r.y,r.z};}
Quat poseQuat(const Vec4f& v){return qnorm({v.x,v.y,v.z,v.w});}
Vec4f poseVec(Quat q){q=qnorm(q);return {q.x,q.y,q.z,q.w};}
Vec3f qtoEuler(Quat q){
 q=qnorm(q);constexpr float toDegrees=57.2957795131F;
 const float sinr=2.0F*(q.w*q.x+q.y*q.z),cosr=1.0F-2.0F*(q.x*q.x+q.y*q.y);
 const float sinp=2.0F*(q.w*q.y-q.z*q.x);
 const float siny=2.0F*(q.w*q.z+q.x*q.y),cosy=1.0F-2.0F*(q.y*q.y+q.z*q.z);
 return {std::atan2(sinr,cosr)*toDegrees,std::asin(std::clamp(sinp,-1.0F,1.0F))*toDegrees,std::atan2(siny,cosy)*toDegrees};
}
Quat qslerp(Quat a,Quat b,float t){float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(d<0){b={-b.x,-b.y,-b.z,-b.w};d=-d;}if(d>.9995F)return qnorm({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});const float angle=std::acos(std::clamp(d,-1.0F,1.0F)),s=std::sin(angle);const float wa=std::sin((1-t)*angle)/s,wb=std::sin(t*angle)/s;return qnorm({a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb,a.w*wa+b.w*wb});}
Vec3f normalizedVector(Vec3f v){const float n=length(v);return n>1e-7F?mul(v,1.0F/n):Vec3f{0,1,0};}
}
Pose evaluateWorldPose(const Skeleton& skeleton,bool bindPose){
 skeleton.validate();Pose out;out.joints.resize(skeleton.joints.size());std::vector<unsigned char> state(skeleton.joints.size(),0);
 const auto visit=[&](auto&& self,std::size_t i)->void{
  if(state[i]==2) return;
  if(state[i]==1) throw std::logic_error("Skeleton hierarchy cycle");
  state[i]=1;const auto& j=skeleton.joints[i];const Vec3f localPosition=bindPose?j.bindPosition:j.position;const Vec3f localRotation=bindPose?j.bindRotationDegrees:j.rotationDegrees;const Quat localQ=qfromEuler(localRotation);
  if(j.parent>=0){const auto p=static_cast<std::size_t>(j.parent);self(self,p);const auto& parent=out.joints[p];const Quat parentQ=poseQuat(parent.rotationQuaternion);const auto parentScale=out.joints[p].scale;const Vec3f scaledLocal{localPosition.x*parentScale.x,localPosition.y*parentScale.y,localPosition.z*parentScale.z};out.joints[i].position=add(parent.position,qrotate(parentQ,scaledLocal));out.joints[i].rotationQuaternion=poseVec(qmul(parentQ,localQ));const auto ps=out.joints[p].scale;const auto ls=bindPose?j.bindScale:j.scale;out.joints[i].scale={ps.x*ls.x,ps.y*ls.y,ps.z*ls.z};}
  else {out.joints[i].position=localPosition;out.joints[i].rotationQuaternion=poseVec(localQ);out.joints[i].scale=bindPose?j.bindScale:j.scale;}
  state[i]=2;
 };
 for(std::size_t i=0;i<skeleton.joints.size();++i) visit(visit,i);
 return out;
}
Pose blendPoses(const Pose& a,const Pose& b,float alpha){
 if(a.joints.size()!=b.joints.size()||!std::isfinite(alpha)||alpha<0||alpha>1)throw std::invalid_argument("Pose blend inputs are incompatible");
 Pose out;out.joints.resize(a.joints.size());for(std::size_t i=0;i<out.joints.size();++i){const auto& x=a.joints[i];const auto& y=b.joints[i];out.joints[i].position=add(x.position,mul(sub(y.position,x.position),alpha));out.joints[i].rotationQuaternion=poseVec(qslerp(poseQuat(x.rotationQuaternion),poseQuat(y.rotationQuaternion),alpha));out.joints[i].scale=add(x.scale,mul(sub(y.scale,x.scale),alpha));}return out;
}
std::vector<Vertex> skinMesh(const Skeleton& skeleton,const std::vector<Vertex>& bindVertices,const std::vector<SkinInfluence>& influences){
 if(bindVertices.size()!=influences.size())throw std::invalid_argument("Every skinned vertex must have one influence record");
 const Pose bind=evaluateWorldPose(skeleton,true),current=evaluateWorldPose(skeleton,false);std::vector<Vertex> out=bindVertices;
 for(std::size_t i=0;i<bindVertices.size();++i){const auto& source=bindVertices[i];const auto& influence=influences[i];float total=0;Vec3f position{},normal{};for(std::size_t k=0;k<4;++k){const float weight=influence.weights[k];if(!std::isfinite(weight)||weight<0)throw std::invalid_argument("Skin weights must be finite and non-negative");if(weight==0)continue;const auto j=influence.joints[k];if(j>=skeleton.joints.size())throw std::out_of_range("Skin joint index is invalid");total+=weight;const auto bindQ=poseQuat(bind.joints[j].rotationQuaternion),currentQ=poseQuat(current.joints[j].rotationQuaternion);const Vec3f bindLocal=qrotate(qconj(bindQ),sub(source.position,bind.joints[j].position));const auto bs=bind.joints[j].scale;const auto cs=current.joints[j].scale;const auto safeDivide=[](float value,float divisor){return std::abs(divisor)>1.0e-7F?value/divisor:0.0F;};const Vec3f local{safeDivide(bindLocal.x,bs.x),safeDivide(bindLocal.y,bs.y),safeDivide(bindLocal.z,bs.z)};const Vec3f scaledLocal{local.x*cs.x,local.y*cs.y,local.z*cs.z};position=add(position,mul(add(current.joints[j].position,qrotate(currentQ,scaledLocal)),weight));const Vec3f adjustedNormal{source.normal.x*safeDivide(bs.x,cs.x),source.normal.y*safeDivide(bs.y,cs.y),source.normal.z*safeDivide(bs.z,cs.z)};normal=add(normal,mul(qrotate(qmul(currentQ,qconj(bindQ)),adjustedNormal),weight));}
  if(total>1e-7F){out[i].position=mul(position,1.0F/total);out[i].normal=normalizedVector(normal);}
 }
 return out;
}
Animator::Animator(Skeleton skeleton):skeleton_(std::move(skeleton)){skeleton_.validate();for(auto& j:skeleton_.joints){j.position=j.bindPosition;j.rotationDegrees=j.bindRotationDegrees;j.scale=j.bindScale;}}
void Animator::play(const Clip& clip,bool looping){clip.validate(skeleton_);clip_=clip;time_=0;looping_=looping;blending_=false;blendDuration_=blendElapsed_=0.0F;}
void Animator::crossFade(const Clip& clip,float blendDuration,bool looping){
 if(!std::isfinite(blendDuration)||blendDuration<=0.0F)throw std::invalid_argument("Animation cross-fade duration must be finite and positive");
 clip.validate(skeleton_);blendSource_=skeleton_;clip_=clip;time_=0;looping_=looping;blendDuration_=blendDuration;blendElapsed_=0.0F;blending_=true;
}
void Animator::stop()noexcept{clip_.reset();time_=0;blending_=false;blendDuration_=blendElapsed_=0.0F;for(auto& j:skeleton_.joints){j.position=j.bindPosition;j.rotationDegrees=j.bindRotationDegrees;j.scale=j.bindScale;}}
void Animator::update(float delta){
    if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Animation delta must be finite and non-negative");
    if(!clip_) return;
    time_+=delta;
    if(clip_->duration>0){if(looping_)time_=std::fmod(time_,clip_->duration);else time_=std::min(time_,clip_->duration);}
    for(auto& j:skeleton_.joints){j.position=j.bindPosition;j.rotationDegrees=j.bindRotationDegrees;j.scale=j.bindScale;}
    for(const auto& track:clip_->tracks){if(!track.positions.empty())skeleton_.joints[track.joint].position=sample(track.positions,time_);if(!track.rotations.empty())skeleton_.joints[track.joint].rotationDegrees=sample(track.rotations,time_);if(!track.scales.empty())skeleton_.joints[track.joint].scale=sample(track.scales,time_);}
    if(blending_){
        blendElapsed_=std::min(blendDuration_,blendElapsed_+delta);
        const float alpha=blendDuration_>0.0F?blendElapsed_/blendDuration_:1.0F;
        for(std::size_t i=0;i<skeleton_.joints.size();++i){
            auto& target=skeleton_.joints[i];const auto& source=blendSource_.joints[i];
            target.position=add(source.position,mul(sub(target.position,source.position),alpha));
            target.scale=add(source.scale,mul(sub(target.scale,source.scale),alpha));
            target.rotationDegrees=qtoEuler(qslerp(qfromEuler(source.rotationDegrees),qfromEuler(target.rotationDegrees),alpha));
        }
        if(blendElapsed_>=blendDuration_)blending_=false;
    }
}
const Skeleton& Animator::skeleton()const noexcept{return skeleton_;}
float Animator::time()const noexcept{return time_;}
AnimatorDebugSnapshot Animator::debugSnapshot()const{
 AnimatorDebugSnapshot snapshot;snapshot.playing=clip_.has_value();snapshot.looping=looping_;snapshot.time=time_;
 if(clip_){snapshot.clipName=clip_->name;snapshot.duration=clip_->duration;}
 snapshot.blending=blending_;snapshot.blendProgress=blending_&&blendDuration_>0.0F?blendElapsed_/blendDuration_:1.0F;
 snapshot.worldPose=evaluateWorldPose(skeleton_);
 snapshot.localPositions.reserve(skeleton_.joints.size());snapshot.localRotations.reserve(skeleton_.joints.size());snapshot.localScales.reserve(skeleton_.joints.size());
 for(const auto& joint:skeleton_.joints){snapshot.localPositions.push_back(joint.position);snapshot.localRotations.push_back(joint.rotationDegrees);snapshot.localScales.push_back(joint.scale);}
 return snapshot;
}
IkResult solveCcd(Skeleton& skeleton,const std::vector<std::size_t>& chain,Vec3f target,std::size_t maxIterations,float tolerance){
    return solveCcdConstrained(skeleton,chain,target,{},maxIterations,tolerance);
}
IkResult solveCcdConstrained(Skeleton& skeleton,const std::vector<std::size_t>& chain,Vec3f target,
                             const std::vector<IkJointConstraint>& constraints,std::size_t maxIterations,float tolerance){
    skeleton.validate();
    if(chain.size()<2)throw std::invalid_argument("IK chain requires at least two joints");
    if(!std::isfinite(target.x)||!std::isfinite(target.y)||!std::isfinite(target.z)||!std::isfinite(tolerance)||tolerance<0)throw std::invalid_argument("IK target/tolerance is invalid");
    for(auto id:chain)if(id>=skeleton.joints.size())throw std::out_of_range("IK chain joint index is invalid");
    for(std::size_t i=1;i<chain.size();++i)if(skeleton.joints[chain[i]].parent!=static_cast<std::int32_t>(chain[i-1]))throw std::invalid_argument("IK chain must follow direct parent links");
    std::vector<const IkJointConstraint*> byJoint(skeleton.joints.size(),nullptr);
    for(const auto& limit:constraints){
        if(limit.joint>=skeleton.joints.size())throw std::out_of_range("IK constraint joint index is invalid");
        const auto finite3=[](Vec3f v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        if(!finite3(limit.minimumRotationDegrees)||!finite3(limit.maximumRotationDegrees)||!std::isfinite(limit.maximumStepDegrees)||limit.maximumStepDegrees<=0.0F||limit.maximumStepDegrees>180.0F||
           limit.minimumRotationDegrees.x>limit.maximumRotationDegrees.x||limit.minimumRotationDegrees.y>limit.maximumRotationDegrees.y||limit.minimumRotationDegrees.z>limit.maximumRotationDegrees.z)
            throw std::invalid_argument("IK rotation limits are invalid");
        if(byJoint[limit.joint])throw std::invalid_argument("Duplicate IK constraint for one joint");
        byJoint[limit.joint]=&limit;
    }
    auto& endPosition=skeleton.joints[chain.back()].position;
    float error=length(sub(target,endPosition));std::size_t iteration=0;
    constexpr float radiansToDegrees=57.2957795131F;
    for(;iteration<maxIterations&&error>tolerance;++iteration){
        for(std::size_t index=chain.size()-1;index-->1;){
            auto& joint=skeleton.joints[chain[index]];
            const Vec3f pivot=joint.position;
            const Vec3f toEnd=sub(endPosition,pivot),toTarget=sub(target,pivot);
            const float a=length(toEnd),b=length(toTarget);if(a<1e-7F||b<1e-7F)continue;
            const Vec3f axis=normalize(Vec3f{toEnd.y*toTarget.z-toEnd.z*toTarget.y,toEnd.z*toTarget.x-toEnd.x*toTarget.z,toEnd.x*toTarget.y-toEnd.y*toTarget.x});
            const float cosine=std::clamp(dot(toEnd,toTarget)/(a*b),-1.0F,1.0F);
            const float angle=std::acos(cosine);if(length(axis)<1e-7F||angle<1e-6F)continue;
            float appliedAngle=angle;
            if(const auto* limit=byJoint[chain[index]]){
                const float fullDegrees=angle*radiansToDegrees,stepDegrees=std::min(fullDegrees,limit->maximumStepDegrees);
                const Vec3f requested{axis.x*stepDegrees,axis.y*stepDegrees,axis.z*stepDegrees};
                const Vec3f old=joint.rotationDegrees;
                Vec3f next{std::clamp(old.x+requested.x,limit->minimumRotationDegrees.x,limit->maximumRotationDegrees.x),
                           std::clamp(old.y+requested.y,limit->minimumRotationDegrees.y,limit->maximumRotationDegrees.y),
                           std::clamp(old.z+requested.z,limit->minimumRotationDegrees.z,limit->maximumRotationDegrees.z)};
                const Vec3f actual=sub(next,old);const float denom=dot(requested,requested);
                const float ratio=denom>1e-8F?std::clamp(dot(actual,requested)/denom,0.0F,1.0F):0.0F;
                appliedAngle=angle*(stepDegrees/std::max(fullDegrees,1e-6F))*ratio;
                joint.rotationDegrees=next;
            }else{
                const float stepDegrees=angle*radiansToDegrees;
                joint.rotationDegrees=add(joint.rotationDegrees,{axis.x*stepDegrees,axis.y*stepDegrees,axis.z*stepDegrees});
            }
            if(appliedAngle<1e-7F)continue;
            for(std::size_t child=index+1;child<chain.size();++child){auto& p=skeleton.joints[chain[child]].position;p=add(pivot,rotateAround(sub(p,pivot),axis,appliedAngle));}
        }
        error=length(sub(target,endPosition));
    }
    return {error<=tolerance,iteration,error};
}
} // namespace mid::animation
