#pragma once
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace espnow { namespace glue {

template<typename T, typename = void>
struct TempReader { static bool read(T*, float& c){ c=0; return false; } };
template<typename T>
struct TempReader<T, std::void_t<decltype(std::declval<T>().getTemperature())>> {
  static bool read(T* o, float& c){ c=o->getTemperature(); return true; }
};
template<typename T>
struct TempReader<T, std::void_t<decltype(std::declval<T>().readCelsius())>> {
  static bool read(T* o, float& c){ c=o->readCelsius(); return true; }
};
template<typename T>
struct TempReader<T, std::void_t<decltype(std::declval<T>().temperatureC())>> {
  static bool read(T* o, float& c){ c=o->temperatureC(); return true; }
};

template<typename T, typename = void>
struct RtcGet { static bool get(T*, uint32_t& t){ t=0; return false; } };
template<typename T>
struct RtcGet<T, std::void_t<decltype(std::declval<T>().getUnix())>> {
  static bool get(T* r, uint32_t& t){ t=r->getUnix(); return true; }
};
template<typename T>
struct RtcGet<T, std::void_t<decltype(std::declval<T>().now())>> {
  static bool get(T* r, uint32_t& t){ t=r->now(); return true; }
};
template<typename T>
struct RtcGet<T, std::void_t<decltype(std::declval<T>().epoch())>> {
  static bool get(T* r, uint32_t& t){ t=r->epoch(); return true; }
};

template<typename T, typename = void>
struct RtcSet { static bool set(T*, uint32_t){ return false; } };
template<typename T>
struct RtcSet<T, std::void_t<decltype(std::declval<T>().setUnix(uint32_t(0)))>> {
  static bool set(T* r, uint32_t t){ r->setUnix(t); return true; }
};
template<typename T>
struct RtcSet<T, std::void_t<decltype(std::declval<T>().setTime(uint32_t(0)))>> {
  static bool set(T* r, uint32_t t){ r->setTime(t); return true; }
};
template<typename T>
struct RtcSet<T, std::void_t<decltype(std::declval<T>().setEpoch(uint32_t(0)))>> {
  static bool set(T* r, uint32_t t){ r->setEpoch(t); return true; }
};

template<typename T, typename = void>
struct CoolingGet { static bool get(T*, uint8_t& m){ m=0; return false; } };
template<typename T>
struct CoolingGet<T, std::void_t<decltype(std::declval<T>().getMode())>> {
  static bool get(T* c, uint8_t& m){ m=(uint8_t)c->getMode(); return true; }
};
template<typename T>
struct CoolingGet<T, std::void_t<decltype(std::declval<T>().mode())>> {
  static bool get(T* c, uint8_t& m){ m=(uint8_t)c->mode(); return true; }
};

template<typename T, typename = void>
struct CoolingSet { static bool set(T*, uint8_t){ return false; } };
template<typename T>
struct CoolingSet<T, std::void_t<decltype(std::declval<T>().setMode(uint8_t(0)))>> {
  static bool set(T* c, uint8_t m){ c->setMode(m); return true; }
};
template<typename T>
struct CoolingSet<T, std::void_t<decltype(std::declval<T>().set(uint8_t(0)))>> {
  static bool set(T* c, uint8_t m){ c->set(m); return true; }
};

template<typename T, typename = void>
struct BuzzerPing { static bool go(T*){ return false; } };
template<typename T>
struct BuzzerPing<T, std::void_t<decltype(std::declval<T>().beep())>> {
  static bool go(T* b){ b->beep(); return true; }
};
template<typename T>
struct BuzzerPing<T, std::void_t<decltype(std::declval<T>().beep(uint16_t(0)))>> {
  static bool go(T* b){ b->beep(50); return true; }
};
template<typename T>
struct BuzzerPing<T, std::void_t<decltype(std::declval<T>().buzz(uint16_t(0)))>> {
  static bool go(T* b){ b->buzz(50); return true; }
};

template<typename T, typename = void>
struct LedPing { static bool go(T*){ return false; } };
template<typename T>
struct LedPing<T, std::void_t<decltype(std::declval<T>().blink(uint8_t(0),uint8_t(0),uint8_t(0)))>> {
  static bool go(T* l){ l->blink(0x10,0x10,0x10); return true; }
};
template<typename T>
struct LedPing<T, std::void_t<decltype(std::declval<T>().set(uint8_t(0),uint8_t(0),uint8_t(0)))>> {
  static bool go(T* l){ l->set(0x10,0x10,0x10); return true; }
};

template<typename T, typename = void>
struct LogRead { static size_t read(T*, uint32_t, uint8_t*, size_t){ return 0; } };
template<typename T>
struct LogRead<T, std::void_t<decltype(std::declval<T>().read(uint32_t(0),(uint8_t*)nullptr,size_t(0)))>> {
  static size_t read(T* l, uint32_t off, uint8_t* buf, size_t max){ return l->read(off, buf, max); }
};
template<typename T>
struct LogRead<T, std::void_t<decltype(std::declval<T>().readChunk(uint32_t(0),(uint8_t*)nullptr,size_t(0)))>> {
  static size_t read(T* l, uint32_t off, uint8_t* buf, size_t max){ return l->readChunk(off, buf, max); }
};

template<typename T, typename = void>
struct RelayGetStates {
  static uint16_t get(T*, uint8_t* out, size_t){ uint32_t b=0; std::memcpy(out,&b,4); return 4; }
};
template<typename T>
struct RelayGetStates<T, std::void_t<decltype(std::declval<T>().getStatesBitmap())>> {
  static uint16_t get(T* r, uint8_t* out, size_t max){ uint32_t b=r->getStatesBitmap(); if(max<4) return 0; std::memcpy(out,&b,4); return 4; }
};
template<typename T>
struct RelayGetStates<T, std::void_t<decltype(std::declval<T>().getStates((uint8_t*)nullptr,size_t(0)))>> {
  static uint16_t get(T* r, uint8_t* out, size_t max){ return (uint16_t)r->getStates(out, max); }
};

template<typename T, typename = void>
struct RelaySet { static bool set(T*, uint8_t, bool, uint16_t){ return false; } };
template<typename T>
struct RelaySet<T, std::void_t<decltype(std::declval<T>().set(uint8_t(0), bool(false)))>> {
  static bool set(T* r, uint8_t ch, bool on, uint16_t ms){ if(ms==0){ r->set(ch,on); return true; } return false; }
};
template<typename T>
struct RelaySet<T, std::void_t<decltype(std::declval<T>().pulse(uint8_t(0), uint16_t(0), bool(false)))>> {
  static bool set(T* r, uint8_t ch, bool on, uint16_t ms){ if(ms==0){ r->set(ch,on); return true; } r->pulse(ch,ms,on); return true; }
};

template<typename T, typename = void>
struct LuxGet { static bool get(T*, uint32_t& v){ v=0; return false; } };
template<typename T>
struct LuxGet<T, std::void_t<decltype(std::declval<T>().lux())>> {
  static bool get(T* v, uint32_t& o){ o=(uint32_t)v->lux(); return true; }
};

template<typename T, typename = void>
struct EnvGet { struct Env{ float tempC; float hum; float press; }; static bool get(T*, Env& e){ e={0,0,0}; return false; } };
template<typename T>
struct EnvGet<T, std::void_t<decltype(std::declval<T>().latest())>> {
  struct Env{ float tempC; float hum; float press; };
  static bool get(T* b, Env& e){ auto x=b->latest(); e.tempC=x.tempC; e.hum=x.hum; e.press=x.press; return true; }
};
template<typename T>
struct EnvGet<T, std::void_t<decltype(std::declval<T>().temperatureC())>> {
  struct Env{ float tempC; float hum; float press; };
  static bool get(T* b, Env& e){ e.tempC=b->temperatureC(); return true; }
};

template<typename T, typename = void>
struct TFLunaGet { struct Raw{ int16_t A_mm; int16_t B_mm; }; static bool get(T*, Raw& r){ r={0,0}; return false; } };
template<typename T>
struct TFLunaGet<T, std::void_t<decltype(std::declval<T>().readRaw())>> {
  struct Raw{ int16_t A_mm; int16_t B_mm; };
  static bool get(T* t, Raw& r){ auto rr=t->readRaw(); r.A_mm=rr.A_mm; r.B_mm=rr.B_mm; return true; }
};
template<typename T>
struct TFLunaGet<T, std::void_t<decltype(std::declval<T>().getA())>> {
  struct Raw{ int16_t A_mm; int16_t B_mm; };
  static bool get(T* t, Raw& r){ r.A_mm=(int16_t)t->getA(); r.B_mm=(int16_t)t->getB(); return true; }
};

template<typename T, typename = void>
struct SensorSetThresh { static bool set(T*, const uint8_t*, uint16_t){ return false; } };
template<typename T>
struct SensorSetThresh<T, std::void_t<decltype(std::declval<T>().setThresholds((const void*)nullptr, size_t(0)))>> {
  static bool set(T* s, const uint8_t* d, uint16_t n){ s->setThresholds(d, n); return true; }
};

template<typename T, typename = void>
struct PmsGetVI { struct VI{ float v; float i; }; static bool get(T*, VI& o){ o={0,0}; return false; } };
template<typename T>
struct PmsGetVI<T, std::void_t<decltype(std::declval<T>().readVI())>> {
  struct VI{ float v; float i; };
  static bool get(T* p, VI& o){ auto r=p->readVI(); o.v=r.v; o.i=r.i; return true; }
};

template<typename T, typename = void>
struct PmsGetSrc { static bool get(T*, uint8_t& o){ o=0; return false; } };
template<typename T>
struct PmsGetSrc<T, std::void_t<decltype(std::declval<T>().getPowerSource())>> {
  static bool get(T* p, uint8_t& o){ o=(uint8_t)p->getPowerSource(); return true; }
};

template<typename T, typename = void>
struct PmsSetGroups { static bool set(T*, const uint8_t*, uint16_t){ return false; } };
template<typename T>
struct PmsSetGroups<T, std::void_t<decltype(std::declval<T>().setGroups((const void*)nullptr, size_t(0)))>> {
  static bool set(T* p, const uint8_t* d, uint16_t n){ p->setGroups(d, n); return true; }
};

}} // namespace espnow::glue
