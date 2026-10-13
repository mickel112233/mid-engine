#include "mid/opengl33_device.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mid::render::gl33;
UInt nextHandle=1;std::size_t instancedDraws=0,ordinaryDraws=0,textureUploads=0;
void genBuffers(Sizei n,UInt* p){for(int i=0;i<n;++i)p[i]=nextHandle++;}
void deleteBuffers(Sizei,const UInt*){} void bindBuffer(Enum,UInt){} void bufferData(Enum,Sizeiptr,const void*,Enum){}
void genVaos(Sizei n,UInt* p){for(int i=0;i<n;++i)p[i]=nextHandle++;}
void deleteVaos(Sizei,const UInt*){} void bindVao(UInt){}
void enableAttrib(UInt){} void disableAttrib(UInt){} void attribPointer(UInt,Int,Enum,Boolean,Sizei,const void*){} void attribDivisor(UInt,UInt){}
UInt createShader(Enum){return nextHandle++;} void shaderSource(UInt,Sizei,const Char* const*,const Int*){} void compileShader(UInt){}
void getShader(UInt,Enum,Int* p){*p=1;} void shaderLog(UInt,Sizei,Sizei*,Char*){} void deleteShader(UInt){}
UInt createProgram(){return nextHandle++;} void attachShader(UInt,UInt){} void linkProgram(UInt){} void getProgram(UInt,Enum,Int* p){*p=1;}
void programLog(UInt,Sizei,Sizei*,Char*){} void deleteProgram(UInt){} void useProgram(UInt){}
Int uniformLocation(UInt,const Char*){return 1;} void uniformMatrix(Int,Sizei,Boolean,const Float*){} void uniform3(Int,Float,Float,Float){}
void uniform4(Int,Float,Float,Float,Float){} void uniform1(Int,Float){} void uniform1i(Int,Int){}
void drawInstanced(Enum,Sizei,Enum,const void*,Sizei n){++instancedDraws;if(n<1)throw std::runtime_error("invalid instance count");}
void drawElements(Enum,Sizei,Enum,const void*){++ordinaryDraws;}
void enable(Enum){} void disable(Enum){} void blendFunc(Enum,Enum){} void depthMask(Boolean){} void depthFunc(Enum){}
void clear(Enum){} void clearColor(Float,Float,Float,Float){} void viewport(Int,Int,Sizei,Sizei){}
void genTextures(Sizei n,UInt* p){for(int i=0;i<n;++i)p[i]=nextHandle++;}
void deleteTextures(Sizei,const UInt*){} void bindTexture(Enum,UInt){}
void texImage(Enum,Int,Int,Sizei,Sizei,Int,Enum,Enum,const void*){++textureUploads;}
void texParameter(Enum,Enum,Int){} void activeTexture(Enum){}
Functions fakeFunctions(){
 Functions f;f.GenBuffers=genBuffers;f.DeleteBuffers=deleteBuffers;f.BindBuffer=bindBuffer;f.BufferData=bufferData;
 f.GenVertexArrays=genVaos;f.DeleteVertexArrays=deleteVaos;f.BindVertexArray=bindVao;
 f.EnableVertexAttribArray=enableAttrib;f.DisableVertexAttribArray=disableAttrib;f.VertexAttribPointer=attribPointer;f.VertexAttribDivisor=attribDivisor;
 f.CreateShader=createShader;f.ShaderSource=shaderSource;f.CompileShader=compileShader;f.GetShaderiv=getShader;f.GetShaderInfoLog=shaderLog;f.DeleteShader=deleteShader;
 f.CreateProgram=createProgram;f.AttachShader=attachShader;f.LinkProgram=linkProgram;f.GetProgramiv=getProgram;f.GetProgramInfoLog=programLog;f.DeleteProgram=deleteProgram;f.UseProgram=useProgram;
 f.GetUniformLocation=uniformLocation;f.UniformMatrix4fv=uniformMatrix;f.Uniform3f=uniform3;f.Uniform4f=uniform4;f.Uniform1f=uniform1;f.Uniform1i=uniform1i;
 f.DrawElementsInstanced=drawInstanced;f.DrawElements=drawElements;f.Enable=enable;f.Disable=disable;f.BlendFunc=blendFunc;f.DepthMask=depthMask;f.DepthFunc=depthFunc;f.Clear=clear;f.ClearColor=clearColor;f.Viewport=viewport;
 f.GenTextures=genTextures;f.DeleteTextures=deleteTextures;f.BindTexture=bindTexture;f.TexImage2D=texImage;f.TexParameteri=texParameter;f.ActiveTexture=activeTexture;
 return f;
}
}
int main(){
 int failures=0;auto check=[&](bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\\n';++failures;}};
 bool rejected=false;try{mid::render::gl33::OpenGL33Device invalid(mid::render::gl33::Functions{});}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"incomplete OpenGL function table is rejected before GPU calls");
 {
  mid::render::gl33::OpenGL33Device device(fakeFunctions());
  mid::render::MeshData mesh;mesh.name="triangle";mesh.vertices.resize(3);mesh.vertices[0].position={0,0,0};mesh.vertices[1].position={1,0,0};mesh.vertices[2].position={0,1,0};mesh.indices={0,1,2};mesh.bounds={{0.5F,0.5F,0},1.0F};device.registerMesh(1,mesh);
  mid::render::Material material;material.name="mat";device.registerMaterial(2,material);
  const auto texture=device.createResource(mid::render::ResourceKind::Texture,0,"test texture");const std::uint8_t pixel[4]={255,0,0,255};device.uploadRgba8(texture.id,1,1,pixel,4);device.bindBaseColorTexture(2,texture.id);
  std::array<float,16> a={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},b=a;b[12]=2;device.setEntityTransform(1,a);device.setEntityTransform(2,b);
  mid::render::RenderPlan plan;plan.opaqueBatches.push_back({1,2,{1,2}});plan.stats.visible=2;
  device.beginFrame();device.submit(plan,mid::render::Camera{},mid::render::DirectionalLight{});device.endFrame();
  check(instancedDraws==1,"native OpenGL device submits one instanced draw for a two-entity batch");
  check(device.lastStats().estimatedInstanceCount==2&&device.lastStats().drawCalls==1,"native render statistics report instance and draw counts");
  check(textureUploads==1,"RGBA8 texture upload reaches OpenGL backend");
 }
 return failures?1:0;
}
