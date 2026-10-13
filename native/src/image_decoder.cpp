#include "mid/image_decoder.hpp"
#include <limits>
#include <stdexcept>
#ifdef MID_ENGINE_WITH_PNG
#include <png.h>
#endif
namespace mid::assets {
namespace {
std::uint16_t u16(std::span<const std::byte> b,std::size_t o){return static_cast<std::uint16_t>(std::to_integer<unsigned>(b[o])|(std::to_integer<unsigned>(b[o+1])<<8U));}
std::uint32_t u32(std::span<const std::byte> b,std::size_t o){return static_cast<std::uint32_t>(std::to_integer<unsigned>(b[o])|(std::to_integer<unsigned>(b[o+1])<<8U)|(std::to_integer<unsigned>(b[o+2])<<16U)|(std::to_integer<unsigned>(b[o+3])<<24U));}
}
bool hasPngSupport() noexcept {
#ifdef MID_ENGINE_WITH_PNG
 return true;
#else
 return false;
#endif
}
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
 throw std::invalid_argument("Unsupported image format. Built-in support: BMP; optional support: PNG.");
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
