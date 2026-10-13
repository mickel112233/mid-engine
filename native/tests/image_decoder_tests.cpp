#include "mid/image_decoder.hpp"
#include <iostream>\n#include <cmath>\n#include <string>
#include <vector>
int main(){
 int failures=0;auto check=[&](bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';++failures;}};
 std::vector<std::byte> bmp(58);auto put16=[&](int o,unsigned v){bmp[o]=std::byte(v&255U);bmp[o+1]=std::byte((v>>8U)&255U);};auto put32=[&](int o,unsigned v){for(int i=0;i<4;++i)bmp[o+i]=std::byte((v>>(8U*static_cast<unsigned>(i)))&255U);};
 bmp[0]=std::byte{'B'};bmp[1]=std::byte{'M'};put32(2,58);put32(10,54);put32(14,40);put32(18,1);put32(22,1);put16(26,1);put16(28,24);put32(34,4);
 bmp[54]=std::byte{3};bmp[55]=std::byte{2};bmp[56]=std::byte{1};bmp[57]=std::byte{0};
 auto image=mid::assets::decodeBmp(bmp);check(image.width==1&&image.height==1,"BMP dimensions decode");check(image.pixels.size()==4&&image.pixels[0]==1&&image.pixels[1]==2&&image.pixels[2]==3&&image.pixels[3]==255,"BMP converts BGR to RGBA and supplies opaque alpha");
 auto dispatched=mid::assets::decodeImage(bmp);check(dispatched.width==1&&dispatched.pixels[0]==1,"image format dispatcher routes BMP data to decoder");
 bool rejected=false;try{(void)mid::assets::decodeBmp(std::span<const std::byte>{});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid BMP is rejected safely");
 if(mid::assets::hasPngSupport()){
  const unsigned char rawPng[]={0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
  std::vector<std::byte> png;for(unsigned char b:rawPng)png.push_back(static_cast<std::byte>(b));
  const auto decoded=mid::assets::decodeImage(png);check(decoded.width==1&&decoded.height==1&&decoded.pixels.size()==4&&decoded.pixels[0]==255&&decoded.pixels[1]==0&&decoded.pixels[2]==0&&decoded.pixels[3]==255,"optional PNG decoder outputs RGBA pixels");
 }
 std::string hdrText="#?RADIANCE\\nFORMAT=32-bit_rle_rgbe\\n\\n-Y 1 +X 1\\n";
 std::vector<std::byte> hdr;for(unsigned char ch:hdrText)hdr.push_back(static_cast<std::byte>(ch));
 hdr.push_back(std::byte{128});hdr.push_back(std::byte{64});hdr.push_back(std::byte{32});hdr.push_back(std::byte{129});
 const auto hdrImage=mid::assets::decodeHdr(hdr);
 check(hdrImage.width==1&&hdrImage.height==1&&hdrImage.pixels.size()==4,"Radiance HDR dimensions and float RGBA buffer decode");
 check(std::abs(hdrImage.pixels[0]-1.0F)<0.0001F&&std::abs(hdrImage.pixels[1]-0.5F)<0.0001F&&std::abs(hdrImage.pixels[2]-0.25F)<0.0001F,"Radiance HDR preserves linear RGBE radiance");
 const auto hdrPreview=mid::assets::decodeImage(hdr);
 check(hdrPreview.width==1&&hdrPreview.pixels.size()==4&&hdrPreview.pixels[3]==255,"generic image dispatcher tone-maps HDR to opaque RGBA8");
 check(mid::assets::hasHdrSupport(),"Radiance HDR decoder is always available");
 bool malformedHdrRejected=false;try{(void)mid::assets::decodeHdr(std::span<const std::byte>{});}catch(const std::invalid_argument&){malformedHdrRejected=true;}
 check(malformedHdrRejected,"invalid HDR input is rejected safely");
 std::vector<std::byte> invalidPng(8);invalidPng[0]=std::byte{137};invalidPng[1]=std::byte{80};invalidPng[2]=std::byte{78};invalidPng[3]=std::byte{71};invalidPng[4]=std::byte{13};invalidPng[5]=std::byte{10};invalidPng[6]=std::byte{26};invalidPng[7]=std::byte{10};bool pngRejected=false;try{(void)mid::assets::decodeImage(invalidPng);}catch(const std::exception&){pngRejected=true;}check(pngRejected,"truncated PNG input is rejected safely");
 return failures?1:0;
}
