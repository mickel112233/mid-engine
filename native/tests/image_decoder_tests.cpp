#include "mid/image_decoder.hpp"
#include <iostream>
#include <vector>
int main(){
 int failures=0;auto check=[&](bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';++failures;}};
 std::vector<std::byte> bmp(58);auto put16=[&](int o,unsigned v){bmp[o]=std::byte(v&255U);bmp[o+1]=std::byte((v>>8U)&255U);};auto put32=[&](int o,unsigned v){for(int i=0;i<4;++i)bmp[o+i]=std::byte((v>>(8U*static_cast<unsigned>(i)))&255U);};
 bmp[0]=std::byte{'B'};bmp[1]=std::byte{'M'};put32(2,58);put32(10,54);put32(14,40);put32(18,1);put32(22,1);put16(26,1);put16(28,24);put32(34,4);
 bmp[54]=std::byte{3};bmp[55]=std::byte{2};bmp[56]=std::byte{1};bmp[57]=std::byte{0};
 auto image=mid::assets::decodeBmp(bmp);check(image.width==1&&image.height==1,"BMP dimensions decode");check(image.pixels.size()==4&&image.pixels[0]==1&&image.pixels[1]==2&&image.pixels[2]==3&&image.pixels[3]==255,"BMP converts BGR to RGBA and supplies opaque alpha");
 bool rejected=false;try{(void)mid::assets::decodeBmp(std::span<const std::byte>{});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid BMP is rejected safely");return failures?1:0;
}
