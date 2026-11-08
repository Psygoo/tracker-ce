#pragma once
#include <cstdint>
#include <cstddef>
#include "PcmEffects.h"
namespace sampleedit {
enum class Mode:std::uint8_t{Mix,Left,Right,Side};
constexpr bool valid(Mode mode){return mode==Mode::Mix||mode==Mode::Left||mode==Mode::Right||mode==Mode::Side;}
constexpr unsigned kBlock=1024;
inline std::int16_t mix(std::int16_t left,std::int16_t right){return static_cast<std::int16_t>((std::int32_t(left)+std::int32_t(right))/2);}
inline std::int16_t convert(std::int16_t left,std::int16_t right,Mode mode){return ce_pcm_mono(left,right,static_cast<unsigned>(mode),1.0f,1);}
inline std::uint32_t crc(const void*data,std::size_t n,std::uint32_t v=0xffffffffu){auto p=static_cast<const std::uint8_t*>(data);while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^((v&1)?0x82f63b78u:0);}return v;}
struct Blocks{std::int16_t left[kBlock],right[kBlock];};
class IO{public:virtual bool input(unsigned channel,std::uint32_t frame,unsigned count,std::int16_t*)=0;virtual bool output(std::uint32_t frame,unsigned count,const std::int16_t*)=0;virtual bool seal()=0;virtual bool verify(std::uint32_t frame,unsigned count,std::int16_t*)=0;virtual ~IO()=default;};
enum class Phase{Mixing,Verifying,Ready,Failed,Cancelled};
class Conversion {
 IO&io_;Blocks&b_;std::uint32_t frames_,position_=0,sum_=0xffffffffu,check_=0xffffffffu;Phase phase_=Phase::Mixing;Mode mode_;float gain_;bool stereo_;std::uint32_t first_,last_;
public:
 Conversion(IO&io,Blocks&b,std::uint32_t frames,Mode mode=Mode::Mix,float gain=1.0f,bool stereo=true,std::uint32_t first=0,std::uint32_t last=UINT32_MAX):io_(io),b_(b),frames_(frames),mode_(mode),gain_(gain),stereo_(stereo),first_(first),last_(last>frames?frames:last){if(!frames||!valid(mode)||first_>=last_)phase_=Phase::Failed;}
 Phase phase()const{return phase_;}std::uint32_t checksum()const{return sum_^0xffffffffu;}
 unsigned progress()const{if(!frames_)return 0;return phase_==Phase::Ready?100:static_cast<unsigned>((std::uint64_t(position_)*45)/frames_)+(phase_==Phase::Verifying?45:0);}
 void cancel(){if(phase_!=Phase::Ready&&phase_!=Phase::Failed)phase_=Phase::Cancelled;}
 void tick(){
  if(phase_!=Phase::Mixing&&phase_!=Phase::Verifying)return;
  const unsigned count=frames_-position_>kBlock?kBlock:frames_-position_;
  if(phase_==Phase::Mixing){
   if(!io_.input(0,position_,count,b_.left)||(stereo_&&!io_.input(1,position_,count,b_.right))){phase_=Phase::Failed;return;}
   for(unsigned i=0;i<count;++i)if(position_+i>=first_&&position_+i<last_)b_.left[i]=ce_pcm_mono(b_.left[i],stereo_?b_.right[i]:b_.left[i],static_cast<unsigned>(mode_),gain_,stereo_);
   sum_=crc(b_.left,count*2,sum_);if(!io_.output(position_,count,b_.left)){phase_=Phase::Failed;return;}
  }else{if(!io_.verify(position_,count,b_.left)){phase_=Phase::Failed;return;}check_=crc(b_.left,count*2,check_);}
  position_+=count;if(position_!=frames_)return;
  if(phase_==Phase::Mixing){if(!io_.seal()){phase_=Phase::Failed;return;}position_=0;phase_=Phase::Verifying;}
  else phase_=sum_==check_?Phase::Ready:Phase::Failed;
 }
};
}
