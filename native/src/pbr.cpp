#include "mid/pbr.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mid::render::pbr {
namespace {
Vec3f add(Vec3f a,Vec3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3f mul(Vec3f a,float s){return {a.x*s,a.y*s,a.z*s};}
Vec3f had(Vec3f a,Vec3f b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
float dot(Vec3f a,Vec3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float len(Vec3f v){return std::sqrt(dot(v,v));}
Vec3f norm(Vec3f v){const float l=len(v);return l>1e-7F?mul(v,1/l):Vec3f{0,1,0};}
Vec3f mix(Vec3f a,Vec3f b,float t){return add(mul(a,1-t),mul(b,t));}
float sat(float x){return std::clamp(x,0.0F,1.0F);}
}
Vec3f evaluate(const Surface& s,const Lighting& l){
 const float values[]={s.albedo.x,s.albedo.y,s.albedo.z,s.metallic,s.roughness,s.ambientOcclusion,l.viewDirection.x,l.viewDirection.y,l.viewDirection.z,l.lightDirection.x,l.lightDirection.y,l.lightDirection.z,l.radiance.x,l.radiance.y,l.radiance.z,l.irradiance.x,l.irradiance.y,l.irradiance.z};
 for(float v:values)if(!std::isfinite(v))throw std::invalid_argument("PBR inputs must be finite");
 if(s.metallic<0||s.metallic>1||s.roughness<0||s.roughness>1||s.ambientOcclusion<0||s.ambientOcclusion>1)throw std::invalid_argument("PBR factors must be in [0,1]");
 const Vec3f n=norm(l.normal),v=norm(l.viewDirection),light=norm(l.lightDirection),h=norm(add(v,light));
 const float ndl=sat(dot(n,light)),ndv=sat(dot(n,v)),ndh=sat(dot(n,h)),vdh=sat(dot(v,h));
 const Vec3f ambient=mul(had(s.albedo,l.irradiance),s.ambientOcclusion);
 if(ndl<=0||ndv<=0)return ambient;
 const float alpha=std::max(.0025F,s.roughness*s.roughness),a2=alpha*alpha,denom=ndh*ndh*(a2-1)+1;
 const float D=a2/(3.14159265F*denom*denom),k=(alpha+1)*(alpha+1)/8;
 const float Gv=ndv/(ndv*(1-k)+k),Gl=ndl/(ndl*(1-k)+k),f=static_cast<float>(std::pow(1-vdh,5));
 const Vec3f f0=mix({.04F,.04F,.04F},s.albedo,s.metallic),F=add(f0,mul({1-f0.x,1-f0.y,1-f0.z},f));
 const Vec3f spec=mul(F,D*Gv*Gl/std::max(4*ndv*ndl,1e-5F));
 const Vec3f diffuse=mul(had({1-F.x,1-F.y,1-F.z},s.albedo),(1-s.metallic)/3.14159265F);
 return add(ambient,mul(had(add(diffuse,spec),l.radiance),ndl));
}
}
