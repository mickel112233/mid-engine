#include "mid/rendering.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures=0;
void check(bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';++failures;}}
template<class E,class F> void throws(F&& f,const char* message){try{f();check(false,message);}catch(const E&){check(true,message);}catch(...){check(false,message);}}
class FakeDevice final : public mid::render::RenderDevice {
public:
    mid::render::ResourceHandle createResource(mid::render::ResourceKind kind,std::size_t,const std::string&) override {
        return {next++,kind};
    }
    void destroyResource(mid::render::ResourceHandle handle) noexcept override { destroyed.push_back(handle.id); }
    void beginFrame() override {}
    void submit(const mid::render::RenderPlan&,const mid::render::Camera&,const mid::render::DirectionalLight&) override {}
    void endFrame() override {}
    mid::render::ResourceId next{1};
    std::vector<mid::render::ResourceId> destroyed;
};
mid::render::Material material(bool transparent=false){mid::render::Material m;m.name="test";m.transparent=transparent;return m;}
void planning() {
    using namespace mid::render;
    Camera camera;
    camera.position={0,0,0}; camera.forward={0,0,-1}; camera.up={0,1,0};
    std::unordered_map<ResourceId,Material> materials{{1,material()},{2,material(true)}};
    std::vector<Renderable> items{
        {1,10,1,{{0,0,-5},1},true,{}},
        {2,10,1,{{0.2F,0,-8},1},true,{}},
        {3,11,2,{{0,0,-7},1},true,{}},
        {4,10,1,{{500,0,-5},1},true,{}},
        {5,10,99,{{0,0,-4},1},true,{}}
    };
    auto plan=RenderPlanner{}.build(camera,items,materials);
    check(plan.stats.submitted==5,"planner counts all submitted objects");
    check(plan.stats.visible==3,"planner culls outside and unresolved materials");
    check(plan.stats.culled==2,"planner counts culled items");
    check(plan.opaqueBatches.size()==1 && plan.opaqueBatches[0].entityIds.size()==2,"opaque items are instancing candidates");
    check(plan.transparentBatches.size()==1,"transparent items have separate batches");
    check(plan.stats.drawCalls==2,"draw calls are estimated from batches");
    items[0].lods={{4.0F,12},{20.0F,13}};
    plan=RenderPlanner{}.build(camera,items,materials);
    check(plan.opaqueBatches[0].mesh==13,"LOD selects mesh by camera distance");
    Camera invalid; invalid.nearPlane=20; invalid.farPlane=1;
    throws<std::invalid_argument>([&]{(void)RenderPlanner{}.build(invalid,items,materials);},"invalid camera is rejected");
}
void lifetime() {
    using namespace mid::render;
    FakeDevice device;
    {
        ResourceLifetime lifetime(device);
        const auto a=lifetime.create(ResourceKind::Buffer,128,"vertex");
        const auto b=lifetime.create(ResourceKind::Texture,256,"albedo");
        check(lifetime.liveCount()==2,"created resources are tracked");
        lifetime.retire(a,5);
        lifetime.collect(4);
        check(device.destroyed.empty(),"resource waits for completion fence");
        lifetime.collect(5);
        check(device.destroyed.size()==1 && device.destroyed[0]==a.id,"retired resource is destroyed after fence");
        check(lifetime.pendingRetireCount()==0,"completed resource leaves retirement queue");
        (void)b;
    }
    check(device.destroyed.size()==2,"shutdown destroys remaining live resources");
}
}
int main(){planning();lifetime();if(failures){std::cerr<<failures<<" rendering test(s) failed\n";return 1;}std::cout<<"Native rendering tests passed\n";return 0;}
