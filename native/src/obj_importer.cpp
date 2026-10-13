#include "mid/obj_importer.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
namespace mid::assets {
namespace {
struct Key {int p;int t;int n;bool operator==(const Key&)const=default;};
struct Hash {std::size_t operator()(const Key& k)const noexcept{return (static_cast<std::size_t>(k.p)*73856093U)^(static_cast<std::size_t>(k.t+1)*19349663U)^(static_cast<std::size_t>(k.n+1)*83492791U);}};
int resolveIndex(int index,std::size_t count,const char* kind){
 if(index==0)throw std::invalid_argument(std::string("OBJ ")+kind+" index cannot be zero");
 const auto resolved=index>0?static_cast<long long>(index)-1:static_cast<long long>(count)+index;
 if(resolved<0||resolved>=static_cast<long long>(count))throw std::invalid_argument(std::string("OBJ ")+kind+" index is out of range");
 return static_cast<int>(resolved);
}
float length(render::Vec3f v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
render::Vec3f sub(render::Vec3f a,render::Vec3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
render::Vec3f cross(render::Vec3f a,render::Vec3f b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
render::Vec3f normalize(render::Vec3f v){const float n=length(v);return n>1e-8F?render::Vec3f{v.x/n,v.y/n,v.z/n}:render::Vec3f{0,1,0};}
Key parseKey(const std::string& token,std::size_t pc,std::size_t tc,std::size_t nc){
 const auto first=token.find('/');const auto second=first==std::string::npos?std::string::npos:token.find('/',first+1);
 const auto p=token.substr(0,first);if(p.empty())throw std::invalid_argument("OBJ face vertex is missing position index");
 int pi=0,ti=-1,ni=-1;try{pi=std::stoi(p);}catch(...){throw std::invalid_argument("OBJ position index is invalid");}
 if(first!=std::string::npos){const auto t=token.substr(first+1,second==std::string::npos?std::string::npos:second-first-1);if(!t.empty()){try{ti=std::stoi(t);}catch(...){throw std::invalid_argument("OBJ texture index is invalid");}}}
 if(second!=std::string::npos){const auto n=token.substr(second+1);if(!n.empty()){try{ni=std::stoi(n);}catch(...){throw std::invalid_argument("OBJ normal index is invalid");}}}
 return {resolveIndex(pi,pc,"position"),ti==-1?-1:resolveIndex(ti,tc,"texture"),ni==-1?-1:resolveIndex(ni,nc,"normal")};
}
}
render::MeshData importObj(std::string_view source,std::string name){
 std::istringstream input{std::string(source)};std::vector<render::Vec3f> positions,normals;std::vector<render::Vec2> uvs;
 render::MeshData mesh;mesh.name=std::move(name);std::unordered_map<Key,std::uint32_t,Hash> unique;
 std::string line;std::size_t lineNo=0;
 while(std::getline(input,line)){++lineNo;std::istringstream row(line);std::string tag;if(!(row>>tag)||tag.starts_with('#'))continue;
  if(tag=="v"){render::Vec3f v;if(!(row>>v.x>>v.y>>v.z))throw std::invalid_argument("Malformed OBJ position at line "+std::to_string(lineNo));positions.push_back(v);}
  else if(tag=="vn"){render::Vec3f v;if(!(row>>v.x>>v.y>>v.z))throw std::invalid_argument("Malformed OBJ normal at line "+std::to_string(lineNo));normals.push_back(normalize(v));}
  else if(tag=="vt"){render::Vec2 v;if(!(row>>v.x>>v.y))throw std::invalid_argument("Malformed OBJ UV at line "+std::to_string(lineNo));uvs.push_back(v);}
  else if(tag=="f"){
   std::vector<std::uint32_t> face;std::string token;
   while(row>>token){if(token.starts_with('#'))break;const Key key=parseKey(token,positions.size(),uvs.size(),normals.size());
    auto found=unique.find(key);if(found==unique.end()){
      if(mesh.vertices.size()>=static_cast<std::size_t>(UINT32_MAX))throw std::invalid_argument("OBJ has too many unique vertices");
      render::Vertex v;v.position=positions[static_cast<std::size_t>(key.p)];if(key.t>=0)v.uv=uvs[static_cast<std::size_t>(key.t)];if(key.n>=0)v.normal=normals[static_cast<std::size_t>(key.n)];
      const auto id=static_cast<std::uint32_t>(mesh.vertices.size());mesh.vertices.push_back(v);unique.emplace(key,id);face.push_back(id);
    }else face.push_back(found->second);
   }
   if(face.size()<3)throw std::invalid_argument("OBJ face needs at least three vertices at line "+std::to_string(lineNo));
   for(std::size_t i=1;i+1<face.size();++i){mesh.indices.push_back(face[0]);mesh.indices.push_back(face[i]);mesh.indices.push_back(face[i+1]);}
  }
 }
 if(mesh.vertices.empty()||mesh.indices.empty())throw std::invalid_argument("OBJ contains no renderable faces");
 // Generate face normals for vertices whose source normal was omitted, accumulating shared corners.
 std::vector<render::Vec3f> generated(mesh.vertices.size(),{0,0,0});
 for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];const auto n=cross(sub(mesh.vertices[ib].position,mesh.vertices[ia].position),sub(mesh.vertices[ic].position,mesh.vertices[ia].position));for(auto id:{ia,ib,ic})if(unique.begin()!=unique.end())generated[id]={generated[id].x+n.x,generated[id].y+n.y,generated[id].z+n.z};}
 for(std::size_t i=0;i<mesh.vertices.size();++i){if(length(mesh.vertices[i].normal)<1e-7F||!std::isfinite(mesh.vertices[i].normal.x))mesh.vertices[i].normal=normalize(generated[i]);}
 render::Vec3f center{};for(const auto& v:mesh.vertices){center.x+=v.position.x;center.y+=v.position.y;center.z+=v.position.z;}
 const float count=static_cast<float>(mesh.vertices.size());center={center.x/count,center.y/count,center.z/count};float radius=0;
 for(const auto& v:mesh.vertices)radius=std::max(radius,length(sub(v.position,center)));
 mesh.bounds={center,radius};mesh.validate();return mesh;
}
} // namespace mid::assets
