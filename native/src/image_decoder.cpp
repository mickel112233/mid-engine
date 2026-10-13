#include "mid/image_decoder.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <setjmp.h>
#include <stdexcept>
#include <string_view>
#ifdef MID_ENGINE_WITH_PNG
#include <png.h>
#endif
#ifdef MID_ENGINE_WITH_JPEG
extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}
#endif
namespace mid::assets {
namespace {
std::uint16_t u16(std::span<const std::byte> b,std::size_t o){return static_cast<std::uint16_t>(std::to_integer<unsigned>(b[o])|(std::to_integer<unsigned>(b[o+1])<<8U));}
std::uint32_t u32(std::span<const std::byte> b,std::size_t o){return static_cast<std::uint32_t>(std::to_integer<unsigned>(b[o])|(std::to_integer<unsigned>(b[o+1])<<8U)|(std::to_integer<unsigned>(b[o+2])<<16U)|(std::to_integer<unsigned>(b[o+3])<<24U));}
std::string_view readLine(std::span<const std::byte> b,std::size_t& cursor){
 const auto start=cursor;while(cursor<b.size()&&std::to_integer<unsigned char>(b[cursor])!='\n')++cursor;
 auto end=cursor;if(cursor<b.size())++cursor;if(end>start&&std::to_integer<unsigned char>(b[end-1])=='\r')--end;
 return {reinterpret_cast<const char*>(b.data()+start),end-start};
}
std::uint8_t toByte(float linear){
 if(!std::isfinite(linear)||linear<=0.0F)return 0;
 const float mapped=linear/(1.0F+linear);
 return static_cast<std::uint8_t>(std::clamp(std::lround(std::pow(mapped,1.0F/2.2F)*255.0F),0L,255L));
}
#ifdef MID_ENGINE_WITH_JPEG
struct JpegError { jpeg_error_mgr base; jmp_buf jump; char message[JMSG_LENGTH_MAX]{}; };
void jpegFailure(j_common_ptr common){
 auto* error=reinterpret_cast<JpegError*>(common->err);
 (*common->err->format_message)(common,error->message);
 longjmp(error->jump,1);
}
ImageRgba8 decodeJpeg(std::span<const std::byte> bytes){
 jpeg_decompress_struct decoder{};JpegError error{};decoder.err=jpeg_std_error(&error.base);error.base.error_exit=jpegFailure;
 unsigned char* volatile raw=nullptr;volatile bool created=false;
 if(setjmp(error.jump)){if(created)jpeg_destroy_decompress(&decoder);std::free(const_cast<unsigned char*>(raw));throw std::invalid_argument(std::string("JPEG decode failed: ")+error.message);}
 jpeg_create_decompress(&decoder);created=true;
 jpeg_mem_src(&decoder,reinterpret_cast<const unsigned char*>(bytes.data()),static_cast<unsigned long>(bytes.size()));
 if(jpeg_read_header(&decoder,TRUE)!=JPEG_HEADER_OK)ERREXIT(&decoder,JERR_NO_IMAGE);
 decoder.out_color_space=JCS_RGB;
 if(!jpeg_start_decompress(&decoder))ERREXIT(&decoder,JERR_CANT_SUSPEND);
 const auto width=decoder.output_width;const auto height=decoder.output_height;const auto channels=decoder.output_components;
 if(width==0||height==0||channels!=3||static_cast<std::uint64_t>(width)*height>std::numeric_limits<std::size_t>::max()/3U)ERREXIT(&decoder,JERR_WIDTH_OVERFLOW);
 const auto count=static_cast<std::size_t>(width)*height*3U;raw=static_cast<unsigned char*>(std::malloc(count));
 if(!raw)ERREXIT(&decoder,JERR_OUT_OF_MEMORY);
 while(decoder.output_scanline<decoder.output_height){JSAMPROW row=const_cast<unsigned char*>(raw)+static_cast<std::size_t>(decoder.output_scanline)*width*3U;if(jpeg_read_scanlines(&decoder,&row,1)!=1)ERREXIT(&decoder,JERR_CANT_SUSPEND);}
 if(!jpeg_finish_decompress(&decoder))ERREXIT(&decoder,JERR_CANT_SUSPEND);
 jpeg_destroy_decompress(&decoder);created=false;
 ImageRgba8 out;out.width=width;out.height=height;out.pixels.resize(static_cast<std::size_t>(width)*height*4U);
 for(std::size_t i=0,j=0;i<count;i+=3,j+=4){out.pixels[j]=raw[i];out.pixels[j+1]=raw[i+1];out.pixels[j+2]=raw[i+2];out.pixels[j+3]=255;}
 std::free(const_cast<unsigned char*>(raw));return out;
}
#endif
}
bool hasPngSupport() noexcept {
#ifdef MID_ENGINE_WITH_PNG
 return true;
#else
 return false;
#endif
}
bool hasJpegSupport() noexcept {
#ifdef MID_ENGINE_WITH_JPEG
 return true;
#else
 return false;
#endif
}
bool hasHdrSupport() noexcept { return true; }
ImageRgba8 decodeImage(std::span<const std::byte> b){
 if(b.size()>=2&&std::to_integer<char>(b[0])=='B'&&std::to_integer<char>(b[1])=='M')return decodeBmp(b);
 constexpr unsigned char signature[8]={137,80,78,71,13,10,26,10};
 if(b.size()>=8){bool match=true;for(std::size_t i=0;i<8;++i)match=match&&std::to_integer<unsigned char>(b[i])==signature[i];if(match){
#ifdef MID_ENGINE_WITH_PNG
  png_image image{};image.version=PNG_IMAGE_VERSION;
  if(!png_image_begin_read_from_memory(&image,b.data(),b.size()))throw std::invalid_argument("PNG header decode failed");
  image.format=PNG_FORMAT_RGBA;
  if(image.width==0||image.height==0||static_cast<std::uint64_t>(image.width)*image.height>std::numeric_limits<std::size_t>::max()/4U){png_image_free(&image);throw std::length_error("PNG dimensions exceed image memory limits");}
  const auto byteCount=PNG_IMAGE_SIZE(image);
  ImageRgba8 out;out.width=image.width;out.height=image.height;out.pixels.resize(static_cast<std::size_t>(byteCount));
  if(!png_image_finish_read(&image,nullptr,out.pixels.data(),0,nullptr)){const std::string error=image.message;png_image_free(&image);throw std::invalid_argument("PNG pixel decode failed: "+error);}
  png_image_free(&image);return out;
#else
  throw std::runtime_error("PNG decoding is unavailable. Install libpng development files and reconfigure CMake.");
#endif
 }}
 if(b.size()>=2&&std::to_integer<unsigned char>(b[0])==0xffU&&std::to_integer<unsigned char>(b[1])==0xd8U){
#ifdef MID_ENGINE_WITH_JPEG
  return decodeJpeg(b);
#else
  throw std::runtime_error("JPEG decoding is unavailable. Install libjpeg development files and reconfigure CMake.");
#endif
 }
 if(b.size()>=10&&std::to_integer<char>(b[0])=='#'&&std::to_integer<char>(b[1])=='?'){
  const auto hdr=decodeHdr(b);ImageRgba8 out{hdr.width,hdr.height,std::vector<std::uint8_t>(hdr.pixels.size())};
  for(std::size_t i=0;i<hdr.pixels.size();i+=4){out.pixels[i]=toByte(hdr.pixels[i]);out.pixels[i+1]=toByte(hdr.pixels[i+1]);out.pixels[i+2]=toByte(hdr.pixels[i+2]);out.pixels[i+3]=255;}
  return out;
 }
 throw std::invalid_argument("Unsupported image format. Built-in support: BMP and Radiance HDR; optional support: PNG and JPEG.");
}
ImageRgba32F decodeHdr(std::span<const std::byte> b){
 if(b.size()<10)throw std::invalid_argument("Truncated Radiance HDR header");
 std::size_t cursor=0;const auto magic=readLine(b,cursor);
 if(magic!="#?RADIANCE"&&magic!="#?RGBE")throw std::invalid_argument("Invalid Radiance HDR signature");
 bool formatFound=false;std::string_view line;
 while(cursor<b.size()){line=readLine(b,cursor);if(line.empty())break;if(line=="FORMAT=32-bit_rle_rgbe")formatFound=true;}
 if(!formatFound)throw std::invalid_argument("Unsupported Radiance HDR encoding; expected 32-bit RGBE");
 if(cursor>=b.size())throw std::invalid_argument("Radiance HDR resolution line is missing");
 line=readLine(b,cursor);
 char ySign=0,xSign=0,yAxis=0,xAxis=0;unsigned height=0,width=0;
 std::string resolution(line);
 if(std::sscanf(resolution.c_str()," %c%c %u %c%c %u",&ySign,&yAxis,&height,&xSign,&xAxis,&width)!=6||
    yAxis!='Y'||xAxis!='X'||(ySign!='-'&&ySign!='+')||(xSign!='-'&&xSign!='+')||width==0||height==0)
  throw std::invalid_argument("Invalid Radiance HDR resolution; expected +/-Y height +/-X width");
 if(static_cast<std::uint64_t>(width)*height>std::numeric_limits<std::size_t>::max()/4U)throw std::length_error("Radiance HDR dimensions overflow image memory");
 ImageRgba32F out{width,height,std::vector<float>(static_cast<std::size_t>(width)*height*4U)};
 std::vector<std::uint8_t> scanline(static_cast<std::size_t>(width)*4U);
 for(unsigned row=0;row<height;++row){
  if(cursor+4>b.size())throw std::invalid_argument("Truncated Radiance HDR pixel data");
  const auto a=std::to_integer<unsigned char>(b[cursor]),c=std::to_integer<unsigned char>(b[cursor+1]),d=std::to_integer<unsigned char>(b[cursor+2]),e=std::to_integer<unsigned char>(b[cursor+3]);
  if(width>=8&&width<=32767&&a==2&&c==2&&(d&0x80U)==0&&((d<<8U)|e)==width){
   cursor+=4;
   for(unsigned channel=0;channel<4;++channel){unsigned x=0;while(x<width){
    if(cursor+2>b.size())throw std::invalid_argument("Truncated Radiance HDR RLE packet");
    const unsigned code=std::to_integer<unsigned char>(b[cursor++]),value=std::to_integer<unsigned char>(b[cursor++]);
    if(code>128U){const unsigned run=code-128U;if(run==0||run>width-x)throw std::invalid_argument("Invalid Radiance HDR RLE run");for(unsigned k=0;k<run;++k)scanline[(static_cast<std::size_t>(x++)*4U)+channel]=static_cast<std::uint8_t>(value);}
    else {const unsigned run=code;if(run==0||run>width-x)throw std::invalid_argument("Invalid Radiance HDR literal run");scanline[static_cast<std::size_t>(x++)*4U+channel]=static_cast<std::uint8_t>(value);for(unsigned k=1;k<run;++k){if(cursor>=b.size())throw std::invalid_argument("Truncated Radiance HDR literal packet");scanline[static_cast<std::size_t>(x++)*4U+channel]=std::to_integer<std::uint8_t>(b[cursor++]);}}
   }}
  }else{
   for(unsigned x=0;x<width;++x){if(x>0){if(cursor+4>b.size())throw std::invalid_argument("Truncated Radiance HDR flat pixel data");}
    for(unsigned channel=0;channel<4;++channel)scanline[static_cast<std::size_t>(x)*4U+channel]=std::to_integer<std::uint8_t>(b[cursor++]);
   }
  }
  const unsigned destY=ySign=='-'?row:height-1U-row;
  for(unsigned x=0;x<width;++x){const unsigned destX=xSign=='+'?x:width-1U-x;const auto si=static_cast<std::size_t>(x)*4U,di=(static_cast<std::size_t>(destY)*width+destX)*4U;const unsigned exponent=scanline[si+3];
   const float scale=exponent==0?0.0F:std::ldexp(1.0F,static_cast<int>(exponent)-136);
   out.pixels[di]=static_cast<float>(scanline[si])*scale;out.pixels[di+1]=static_cast<float>(scanline[si+1])*scale;out.pixels[di+2]=static_cast<float>(scanline[si+2])*scale;out.pixels[di+3]=1.0F;
  }
 }
 return out;
}
ImageRgba8 decodeBmp(std::span<const std::byte> b){
    if(b.size()<54||std::to_integer<char>(b[0])!='B'||std::to_integer<char>(b[1])!='M')throw std::invalid_argument("Invalid BMP signature or truncated header");
    const auto offset=u32(b,10),header=u32(b,14);
    if(header<40||14ULL+header>b.size())throw std::invalid_argument("Unsupported BMP DIB header");
    const auto widthSigned=static_cast<std::int32_t>(u32(b,18)),heightSigned=static_cast<std::int32_t>(u32(b,22));
    const auto planes=u16(b,26); const auto bits=u16(b,28); const auto compression=u32(b,30);
    if(widthSigned<=0||heightSigned==0||heightSigned==std::numeric_limits<std::int32_t>::min()||planes!=1||(bits!=24&&bits!=32)||compression!=0)
        throw std::invalid_argument("Only uncompressed 24/32-bit BMP images are supported");
    const auto width=static_cast<std::uint32_t>(widthSigned),height=static_cast<std::uint32_t>(heightSigned<0?-heightSigned:heightSigned);
    if(static_cast<std::uint64_t>(width)*height>std::numeric_limits<std::size_t>::max()/4U)throw std::length_error("BMP dimensions overflow image memory");
    const std::uint64_t row=((static_cast<std::uint64_t>(width)*bits+31U)/32U)*4U;
    if(static_cast<std::uint64_t>(offset)+row*height>b.size())throw std::invalid_argument("BMP pixel data is truncated");
    ImageRgba8 image{width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*4U)};
    const bool topDown=heightSigned<0;const std::size_t channels=bits/8U;
    for(std::uint32_t y=0;y<height;++y){const auto srcY=topDown?y:(height-1U-y);const auto* src=b.data()+offset+static_cast<std::size_t>(row*srcY);
        for(std::uint32_t x=0;x<width;++x){const auto si=static_cast<std::size_t>(x)*channels,di=(static_cast<std::size_t>(y)*width+x)*4U;
            image.pixels[di]=std::to_integer<std::uint8_t>(src[si+2]);image.pixels[di+1]=std::to_integer<std::uint8_t>(src[si+1]);image.pixels[di+2]=std::to_integer<std::uint8_t>(src[si]);image.pixels[di+3]=channels==4?std::to_integer<std::uint8_t>(src[si+3]):255U;
        }
    }
    return image;
}
}
