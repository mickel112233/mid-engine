#include "mid/opengl33_device.hpp"
#include <iostream>
#include <stdexcept>
int main(){
 bool rejected=false;
 try{mid::render::gl33::OpenGL33Device device(mid::render::gl33::Functions{});}
 catch(const std::invalid_argument&){rejected=true;}
 if(!rejected){std::cerr<<"FAIL: incomplete OpenGL dispatch must be rejected before GPU calls\\n";return 1;}
 std::cout<<"OpenGL backend validation test passed\\n";return 0;
}
