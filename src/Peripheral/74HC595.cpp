/**************************************************************
 *  Project : EasyDriveway
 *  File    : 74HC595.cpp
 **************************************************************/
#include "74HC595.h"
SR74HC595::SR74HC595(LogFS* log):_log(log){ for(uint8_t i=0;i<32;i++){_map[i]=i;} }
bool SR74HC595::begin(int ser,int sck,int rck,int oe,int mr,uint8_t chips){
  _pinSER=ser;_pinSCK=sck;_pinRCK=rck;_pinOE=oe;_pinMR=mr;_chips=chips?chips:1;_shadow=0;
  pinMode(_pinSER,OUTPUT);pinMode(_pinSCK,OUTPUT);pinMode(_pinRCK,OUTPUT);
  digitalWrite(_pinSER,LOW);digitalWrite(_pinSCK,LOW);digitalWrite(_pinRCK,LOW);
  if(_pinOE>=0){pinMode(_pinOE,OUTPUT);digitalWrite(_pinOE,LOW);}
  if(_pinMR>=0){pinMode(_pinMR,OUTPUT);digitalWrite(_pinMR,HIGH);}
  resetMapping();
  shiftOutPhysical(0);latch();
  _ok=true;
  return true;
}
bool SR74HC595::beginAuto(uint8_t chips){
#if defined(SR_SER_PIN) && defined(SR_SCK_PIN) && defined(SR_RCK_PIN)
  int ser=SR_SER_PIN,sck=SR_SCK_PIN,rck=SR_RCK_PIN;
  int oe = (defined(SR_OE_PIN)? SR_OE_PIN : -1);
  int mr = (defined(SR_MR_PIN)? SR_MR_PIN : -1);
  uint8_t n=chips?chips:1;
  #if defined(NVS_ROLE_REMU) && defined(REL_CH_COUNT)
    if(!chips) n=(REL_CH_COUNT+7)/8; // 16→2 chips for REMU
  #endif
  return begin(ser,sck,rck,oe,mr,n);
#else
  _ok=false;return false;
#endif
}
void SR74HC595::setEnabled(bool enable){
  _enabled=enable;
  if(_pinOE>=0){digitalWrite(_pinOE,enable?LOW:HIGH);}
}
void SR74HC595::clear(){
  _shadow=0;
  shiftOutPhysical(0);
  latch();
}
void SR74HC595::writeLogical(uint16_t logicalIndex,bool on){
  if(!_ok) return;
  if(logicalIndex>=bitCount()) return;
  uint32_t mask=(1UL<<logicalIndex);
  if(on){_shadow|=mask;}else{_shadow&=~mask;}
  // remap to physical
  uint32_t phys=0;
  uint16_t total=bitCount();
  for(uint16_t l=0;l<total;l++){
    if((_shadow>>l)&1UL){
      uint16_t p=mapToPhysical(l);
      phys|=(1UL<<p);
    }
  }
  shiftOutPhysical(phys);
  latch();
}
bool SR74HC595::assignLogicalToPhysical(uint16_t logicalIndex,uint16_t physicalIndex){
  if(!_ok) return false;
  if(logicalIndex>=bitCount()) return false;
  if(physicalIndex>=bitCount()) return false;
  _map[logicalIndex]=physicalIndex;
  return true;
}
void SR74HC595::resetMapping(){
  uint16_t total=bitCount();
  for(uint16_t i=0;i<32;i++){ _map[i]= (i<total)? i : i; }
}
void SR74HC595::writeMask(uint32_t mask){
  if(!_ok) return;
  uint16_t total=bitCount();
  uint32_t keep=(total>=32)?0xFFFFFFFFUL:((1UL<<total)-1UL);
  _shadow = mask & keep;
  uint32_t phys=0;
  for(uint16_t l=0;l<total;l++){
    if((_shadow>>l)&1UL){ uint16_t p=mapToPhysical(l); phys|=(1UL<<p); }
  }
  shiftOutPhysical(phys);
  latch();
}
void SR74HC595::pulse(int pin){
  digitalWrite(pin,HIGH);
  digitalWrite(pin,LOW);
}
void SR74HC595::latch(){
  pulse(_pinRCK);
}
void SR74HC595::shiftOutPhysical(uint32_t physicalMask){
  uint16_t total=bitCount();
  for(uint16_t i=0;i<total;i++){
    bool bit=(physicalMask>>i)&1UL;
    digitalWrite(_pinSER,bit?HIGH:LOW);
    pulse(_pinSCK);
  }
}
uint16_t SR74HC595::mapToPhysical(uint16_t logical) const{
  return _map[logical];
}

