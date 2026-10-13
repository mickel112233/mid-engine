#include "mid/model_importer.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef MID_ENGINE_WITH_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif
namespace mid::assets {
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
 const auto visitNode=[&](auto&& self,const aiNode* sourceNode,std::int32_t parent)->void{
  ImportedNode node;node.name=sourceNode->mName.C_Str();node.parent=parent;const auto& m=sourceNode->mTransformation;
  node.localTransform={m.a1,m.b1,m.c1,m.d1,m.a2,m.b2,m.c2,m.d2,m.a3,m.b3,m.c3,m.d3,m.a4,m.b4,m.c4,m.d4};
  for(unsigned int i=0;i<sourceNode->mNumMeshes;++i){const auto sourceIndex=sourceNode->mMeshes[i];if(sourceIndex<sourceMeshToImported.size()&&sourceMeshToImported[sourceIndex]>=0)node.meshIndices.push_back(static_cast<std::uint32_t>(sourceMeshToImported[sourceIndex]));}
  if(out.nodes.size()>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("Imported model node hierarchy is too large");
  const auto thisIndex=static_cast<std::int32_t>(out.nodes.size());out.nodes.push_back(std::move(node));
  for(unsigned int i=0;i<sourceNode->mNumChildren;++i)self(self,sourceNode->mChildren[i],thisIndex);
 };
 out.nodes.reserve(scene->mNumMeshes+1U);visitNode(visitNode,scene->mRootNode,-1);
 if(scene->mNumTextures>0)out.warnings.emplace_back("Embedded textures are reported by Assimp but must be extracted/decoded by the asset pipeline.");
 return out;
#endif
}
}
