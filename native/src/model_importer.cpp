#include "mid/model_importer.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <unordered_map>
#ifdef MID_ENGINE_WITH_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif
namespace mid::assets {
#ifdef MID_ENGINE_WITH_ASSIMP
namespace {
render::Vec3f quaternionToEulerDegrees(const aiQuaternion& q){
 const float sinr=2.0F*(q.w*q.x+q.y*q.z),cosr=1.0F-2.0F*(q.x*q.x+q.y*q.y);
 const float sinp=2.0F*(q.w*q.y-q.z*q.x);
 const float siny=2.0F*(q.w*q.z+q.x*q.y),cosy=1.0F-2.0F*(q.y*q.y+q.z*q.z);
 constexpr float degrees=57.2957795131F;
 return {std::atan2(sinr,cosr)*degrees,std::asin(std::clamp(sinp,-1.0F,1.0F))*degrees,std::atan2(siny,cosy)*degrees};
}
}
#endif

bool hasAssimpModelSupport() noexcept {
#ifdef MID_ENGINE_WITH_ASSIMP
 return true;
#else
 return false;
#endif
}
ImportedModel importModelFile(const std::filesystem::path& path) {
#ifndef MID_ENGINE_WITH_ASSIMP
 (void)path;
 throw std::runtime_error("Model import support is not built. Install Assimp and reconfigure to enable glTF/GLB/FBX import.");
#else
 Assimp::Importer importer;
 const aiScene* scene=importer.ReadFile(path.string(),aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_JoinIdenticalVertices|aiProcess_ImproveCacheLocality|aiProcess_SortByPType|aiProcess_ValidateDataStructure);
 if(!scene||!scene->mRootNode) throw std::runtime_error("Assimp model import failed: "+std::string(importer.GetErrorString()));
 ImportedModel out; out.meshes.reserve(scene->mNumMeshes); out.materials.reserve(scene->mNumMaterials);
 out.embeddedTextures.reserve(scene->mNumTextures);
 for(unsigned int i=0;i<scene->mNumTextures;++i){
  const aiTexture* source=scene->mTextures[i]; if(!source){out.embeddedTextures.emplace_back();continue;}
  ImportedTexture texture; texture.name=source->mFilename.length?source->mFilename.C_Str():"*"+std::to_string(i);
  texture.formatHint=source->achFormatHint; texture.width=source->mWidth; texture.height=source->mHeight;
  texture.compressed=source->mHeight==0;
  if(texture.compressed){
   texture.bytes.resize(source->mWidth);
   const auto* raw=reinterpret_cast<const std::uint8_t*>(source->pcData);
   if(source->mWidth>0&&raw)std::copy(raw,raw+source->mWidth,texture.bytes.begin());
  }else{
   const std::uint64_t pixelCount=static_cast<std::uint64_t>(source->mWidth)*source->mHeight;
   if(pixelCount>static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()/4U))throw std::length_error("Embedded texture dimensions exceed memory limits");
   texture.bytes.resize(static_cast<std::size_t>(pixelCount)*4U);
   for(std::size_t p=0;p<static_cast<std::size_t>(pixelCount);++p){const auto& texel=source->pcData[p];const auto o=p*4U;texture.bytes[o]=texel.r;texture.bytes[o+1]=texel.g;texture.bytes[o+2]=texel.b;texture.bytes[o+3]=texel.a;}
  }
  out.embeddedTextures.push_back(std::move(texture));
 }
 for(unsigned int i=0;i<scene->mNumMaterials;++i) {
  const aiMaterial* source=scene->mMaterials[i]; ModelMaterial target;
  aiString name; if(source->Get(AI_MATKEY_NAME,name)==AI_SUCCESS) target.material.name=name.C_Str();
  aiColor4D color; if(aiGetMaterialColor(source,AI_MATKEY_COLOR_DIFFUSE,&color)==AI_SUCCESS) target.material.baseColor={color.r,color.g,color.b,color.a};
  float factor=0; if(source->Get("$mat.metallicFactor",0,0,factor)==AI_SUCCESS) target.material.metallic=std::clamp(factor,0.0F,1.0F);
  if(source->Get("$mat.roughnessFactor",0,0,factor)==AI_SUCCESS) target.material.roughness=std::clamp(factor,0.0F,1.0F);
  target.material.transparent=target.material.baseColor.w<0.999F;
  for(unsigned int t=0;t<source->GetTextureCount(aiTextureType_BASE_COLOR);++t){aiString p;if(source->GetTexture(aiTextureType_BASE_COLOR,t,&p)==AI_SUCCESS)target.texturePaths.emplace_back(p.C_Str());}
  if(target.texturePaths.empty()) for(unsigned int t=0;t<source->GetTextureCount(aiTextureType_DIFFUSE);++t){aiString p;if(source->GetTexture(aiTextureType_DIFFUSE,t,&p)==AI_SUCCESS)target.texturePaths.emplace_back(p.C_Str());}
  for(auto& texture:target.texturePaths){if(texture.empty()||texture.front()=='*')continue;const std::filesystem::path texturePath(texture);if(texturePath.is_relative())texture=(path.parent_path()/texturePath).lexically_normal().string();}
  target.material.validate();out.materials.push_back(std::move(target));
 }
 std::vector<std::int32_t> sourceMeshToImported(scene->mNumMeshes,-1);
 for(unsigned int i=0;i<scene->mNumMeshes;++i) {
  const aiMesh* source=scene->mMeshes[i]; if(!source->HasPositions()||source->mNumVertices==0)continue;
  render::MeshData mesh; aiString name; source->mName.length?mesh.name=source->mName.C_Str():mesh.name="Mesh_"+std::to_string(i);
  mesh.vertices.reserve(source->mNumVertices);
  render::Vec3f center{};
  for(unsigned int v=0;v<source->mNumVertices;++v){const auto& p=source->mVertices[v];render::Vertex vertex;vertex.position={p.x,p.y,p.z};center.x+=p.x;center.y+=p.y;center.z+=p.z;
   if(source->HasNormals()){const auto& n=source->mNormals[v];vertex.normal={n.x,n.y,n.z};}
   if(source->HasTextureCoords(0)){vertex.uv={source->mTextureCoords[0][v].x,source->mTextureCoords[0][v].y};}
   if(source->HasVertexColors(0)){const auto& col=source->mColors[0][v];vertex.color={col.r,col.g,col.b,col.a};}
   mesh.vertices.push_back(vertex);
  }
  const float inv=1.0F/static_cast<float>(mesh.vertices.size());center.x*=inv;center.y*=inv;center.z*=inv;float radius=0;
  for(const auto& v:mesh.vertices){const float x=v.position.x-center.x,y=v.position.y-center.y,z=v.position.z-center.z;radius=std::max(radius,std::sqrt(x*x+y*y+z*z));}
  mesh.bounds={center,radius};
  for(unsigned int f=0;f<source->mNumFaces;++f){const auto& face=source->mFaces[f];if(face.mNumIndices!=3)continue;for(unsigned int k=0;k<3;++k)mesh.indices.push_back(face.mIndices[k]);}
  mesh.validate();sourceMeshToImported[i]=static_cast<std::int32_t>(out.meshes.size());out.meshMaterialIndices.push_back(source->mMaterialIndex);out.meshes.push_back(std::move(mesh));
 }
 if(out.meshes.empty()) throw std::runtime_error("Model import produced no triangle meshes");
 std::vector<const aiNode*> sourceNodes;sourceNodes.reserve(scene->mNumMeshes+1U);
 const auto visitNode=[&](auto&& self,const aiNode* sourceNode,std::int32_t parent)->void{
  ImportedNode node;node.name=sourceNode->mName.C_Str();node.parent=parent;const auto& m=sourceNode->mTransformation;
  node.localTransform={m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4};
  for(unsigned int i=0;i<sourceNode->mNumMeshes;++i){const auto sourceIndex=sourceNode->mMeshes[i];if(sourceIndex<sourceMeshToImported.size()&&sourceMeshToImported[sourceIndex]>=0)node.meshIndices.push_back(static_cast<std::uint32_t>(sourceMeshToImported[sourceIndex]));}
  if(out.nodes.size()>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("Imported model node hierarchy is too large");
  const auto thisIndex=static_cast<std::int32_t>(out.nodes.size());out.nodes.push_back(std::move(node));sourceNodes.push_back(sourceNode);
  for(unsigned int i=0;i<sourceNode->mNumChildren;++i)self(self,sourceNode->mChildren[i],thisIndex);
 };
 out.nodes.reserve(scene->mNumMeshes+1U);visitNode(visitNode,scene->mRootNode,-1);
 out.skeleton.joints.reserve(sourceNodes.size());
 for(std::size_t i=0;i<sourceNodes.size();++i){aiVector3D scale,position;aiQuaternion rotation;sourceNodes[i]->mTransformation.Decompose(scale,rotation,position);const auto euler=quaternionToEulerDegrees(rotation);const render::Vec3f p{position.x,position.y,position.z};const render::Vec3f s{scale.x,scale.y,scale.z};out.skeleton.joints.push_back({out.nodes[i].name,out.nodes[i].parent,p,p,euler,euler,s,s});if(std::abs(scale.x-1.0F)>1e-4F||std::abs(scale.y-1.0F)>1e-4F||std::abs(scale.z-1.0F)>1e-4F)out.warnings.emplace_back("Non-unit node scale is preserved in node matrices but not represented in the skeletal pose helper.");}
 out.skeleton.validate();
 std::unordered_map<std::string,std::size_t> nodeByName;for(std::size_t i=0;i<out.nodes.size();++i)nodeByName.emplace(out.nodes[i].name,i);
 for(unsigned int i=0;i<scene->mNumAnimations;++i){const aiAnimation* source=scene->mAnimations[i];animation::Clip clip;clip.name=source->mName.length?source->mName.C_Str():"Animation_"+std::to_string(i);const double ticks=source->mTicksPerSecond>0?source->mTicksPerSecond:25.0;clip.duration=static_cast<float>(source->mDuration/ticks);
  for(unsigned int channelIndex=0;channelIndex<source->mNumChannels;++channelIndex){const aiNodeAnim* channel=source->mChannels[channelIndex];const auto node=nodeByName.find(channel->mNodeName.C_Str());if(node==nodeByName.end()){out.warnings.emplace_back("Animation channel references a node absent from the imported hierarchy.");continue;}animation::JointTrack track;track.joint=node->second;
   for(unsigned int k=0;k<channel->mNumPositionKeys;++k){const auto& key=channel->mPositionKeys[k];track.positions.push_back({std::clamp(static_cast<float>(key.mTime/ticks),0.0F,clip.duration),{key.mValue.x,key.mValue.y,key.mValue.z}});}
   for(unsigned int k=0;k<channel->mNumRotationKeys;++k){const auto& key=channel->mRotationKeys[k];track.rotations.push_back({std::clamp(static_cast<float>(key.mTime/ticks),0.0F,clip.duration),quaternionToEulerDegrees(key.mValue)});}
   for(unsigned int k=0;k<channel->mNumScalingKeys;++k){const auto& key=channel->mScalingKeys[k];track.scales.push_back({std::clamp(static_cast<float>(key.mTime/ticks),0.0F,clip.duration),{key.mValue.x,key.mValue.y,key.mValue.z}});}
   clip.tracks.push_back(std::move(track));
  }
  clip.validate(out.skeleton);out.animations.push_back(std::move(clip));
 }

 return out;
#endif
}
}
