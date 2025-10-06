/**************************************************************
 *  Project  : ICM (Interface Control Module)
 *  Headers  : WiFiManager.h
 *  Note     : Readability regrouping only — no API changes.
 *             All macros, types, methods preserved verbatim.
 **************************************************************/
// ============================== WiFiManager.h ==============================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H


class WiFiManager {
public: // ======================= LIFECYCLE & CORE =======================
  enum NetMode : uint8_t { NET_AP=0, NET_STA=1 };

  WiFiManager() {}

  void begin();


  // --- add in the public section ---
  void forceAP();           // start AP immediately (wraps private startAccessPoint)
  bool isAPOn() const;      // query AP state

};


#endif // WIFI_MANAGER_H