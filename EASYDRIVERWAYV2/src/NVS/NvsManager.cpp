/**************************************************************
 *  Project : EasyDriveway
 *  File    : NvsManager.cpp
 **************************************************************/
#include "NvsManager.h"

// Format a 48-bit MAC as 12 uppercase hex chars (no colons)
static String mac12_from_efuse() {
  uint64_t m = ESP.getEfuseMac();  // 0xAABBCCDDEEFF (LSB-first on ESP32)
  char buf[13];
  uint8_t bytes[6] = {
    (uint8_t)(m >> 40), (uint8_t)(m >> 32), (uint8_t)(m >> 24),
    (uint8_t)(m >> 16), (uint8_t)(m >> 8 ), (uint8_t)(m >> 0 )
  };
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
           bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5]);
  return String(buf);
}
NvsManager::NvsManager() : namespaceName(CONFIG_PARTITION) {}
NvsManager::~NvsManager() {
    end();
}
void NvsManager::RestartSysDelayDown(unsigned long delayTime) {
    unsigned long startTime = millis();
    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Restarting the Device in: ");
        DEBUG_PRINT(delayTime / 1000);
        DEBUG_PRINTLN(" Sec");
    }
    unsigned long interval = delayTime / 32;
    if (DEBUGMODE) {
        for (int i = 0; i < 32; i++) {
            DEBUG_PRINT("#");
            delay(interval);
            esp_task_wdt_reset();
        }
        DEBUG_PRINTLN();
    }
    if (DEBUGMODE) {
        DEBUG_PRINTLN("Restarting now...");
    }
    simulatePowerDown();
}
void NvsManager::RestartSysDelay(unsigned long delayTime) {
    unsigned long startTime = millis();
    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Restarting the Device in: ");
        DEBUG_PRINT(delayTime / 1000);
        DEBUG_PRINTLN(" Sec");
    }
    unsigned long interval = delayTime / 32;
    if (DEBUGMODE) {
        for (int i = 0; i < 32; i++) {
            DEBUG_PRINT("#");
            delay(interval);
            esp_task_wdt_reset();
        }
        DEBUG_PRINTLN();
    }
    if (DEBUGMODE) {
        DEBUG_PRINTLN("Restarting now...");
    }
    //simulatePowerDown();
    ESP.restart();
}
void NvsManager::CountdownDelay(unsigned long delayTime) {
    unsigned long startTime = millis();
    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Waiting User Action: ");
        DEBUG_PRINT(delayTime / 1000);
        DEBUG_PRINTLN(" Sec");
    }
    if (DEBUGMODE) {
        unsigned long interval = delayTime / 32;
        for (int i = 0; i < 32; i++) {
            DEBUG_PRINT("#");
            delay(interval);
            esp_task_wdt_reset();
        }
        DEBUG_PRINTLN();
    }
}
void NvsManager::simulatePowerDown() {
    esp_sleep_enable_timer_wakeup(1000000);
    esp_deep_sleep_start();
}
void NvsManager::startPreferencesReadWrite() {
    pref.begin(CONFIG_PARTITION, false);
    DEBUG_PRINTLN("Preferences opened in write mode.");
}
void NvsManager::startPreferencesRead() {
    pref.begin(CONFIG_PARTITION, true);
    DEBUG_PRINTLN("Preferences opened in read mode.");
}
void NvsManager::begin() {
    DEBUG_PRINTLN("###########################################################");
    DEBUG_PRINTLN("#               Starting CONFIG Manager                   #");
    DEBUG_PRINTLN("###########################################################");
    startPreferencesReadWrite();
    bool resetFlag = GetBool(RESET_FLAG_KEY, true);
    if (resetFlag) {
        DEBUG_PRINTLN("ConfigManager: Initializing the device...");
        initializeDefaults();
        RestartSysDelay(7000);
    } else {
        DEBUG_PRINTLN("ConfigManager: Using existing configuration...");
    }
}
bool NvsManager::getResetFlag() {
    esp_task_wdt_reset();
    bool value = pref.getBool(RESET_FLAG_KEY, true);
    return value;
}
void NvsManager::end() {
    pref.end();
}
void NvsManager::initializeDefaults() {
    initializeVariables();
}
void NvsManager::initializeVariables() {
  const String mac12   = mac12_from_efuse();
  const String macTail = mac12.substring(6);
  const String uniqId  = String("DL-") + mac12;
  const String uniqNm  = String("DL_") + macTail;
  PutInt   (NVS_KEY_KIND,   (int)NVS_DEF_KIND);
  PutString(NVS_KEY_DEVID,  uniqId);
  PutString(NVS_KEY_HWREV,  NVS_DEF_HWREV);
  PutString(NVS_KEY_SWVER,  NVS_DEF_SWVER);
  PutString(NVS_KEY_BUILD,  NVS_DEF_BUILD);
  PutString(NVS_KEY_DEFNM,  uniqNm);
  PutInt   (NVS_KEY_CHAN,   (int)NVS_DEF_CHAN);
  PutString(NVS_KEY_ICMMAC, NVS_DEF_ICMMAC);
  PutBool  (NVS_KEY_PAIRED, (bool)NVS_DEF_PAIRED);
  PutInt   (NVS_KEY_TOKEN,  (int)NVS_DEF_TOKEN);
  PutBool(NVS_KEY_LEDDIS, (bool)NVS_DEF_LEDDIS);
  PutBool(NVS_KEY_BUZDIS, (bool)NVS_DEF_BUZDIS);
  PutBool(NVS_KEY_LEDDIS, (bool)NVS_DEF_LEDDIS);
  PutBool(NVS_KEY_BUZDIS, (bool)NVS_DEF_BUZDIS);
  PutBool(NVS_KEY_RGBALW, (bool)NVS_DEF_RGBALW);
  PutBool(NVS_KEY_RGBFBK, (bool)NVS_DEF_RGBFBK);
  PutBool(NVS_KEY_BUZAHI, (bool)NVS_DEF_BUZAHI);
  PutBool(NVS_KEY_BUZFBK, (bool)NVS_DEF_BUZFBK);
#if defined(NVS_ROLE_ICM)
  PutString(NVS_KEY_BLENM,  uniqNm);
  PutString(NVS_KEY_APSID,  uniqNm);
  PutString(NVS_KEY_APKEY,  NVS_DEF_APKEY);
  PutString(NVS_KEY_STSID,  NVS_DEF_STSID);
  PutString(NVS_KEY_STKEY,  NVS_DEF_STKEY);
  const uint64_t ef  = ESP.getEfuseMac();
  const uint32_t mix = (uint32_t)(ef ^ (ef >> 21) ^ (ef >> 33));
  const uint32_t rot = (mix << 7) | (mix >> (32 - 7));
  const uint32_t blePass = ((mix * 2654435761UL) % 900000) + 100000;
  const uint32_t pin6    = ((rot ^ 0x5A5A5A5AUL)   % 900000) + 100000;
  PutInt(NVS_KEY_BLEPK,  (int)blePass);
  PutInt(NVS_KEY_PIN___, (int)pin6);
  PutString(NVS_KEY_TOPO__,  NVS_DEF_TOPO__);
  PutString(NVS_KEY_SLMACS,  NVS_DEF_SLMACS);
  PutString(ICM_UI_THM_KEY, ICM_UI_THM_DEF);
  PutInt   (ICM_SEQ_KEY,    (int)ICM_SEQ_DEF);
  PutInt   (ICM_PTTL_KEY,   (int)ICM_PTTL_DEF);
  PutInt   (ICM_PMAX_KEY,   (int)ICM_PMAX_DEF);
  PutBool  (ICM_TSAVE_KEY,  (bool)ICM_TSAVE_DEF);
  PutString(ICM_XFMT_KEY,   ICM_XFMT_DEF);
#endif
#if defined(NVS_ROLE_PMS)
  PutBool(PMS_PAIRING_KEY, (bool)PMS_PAIRING_DEF);
  PutBool(PMS_PAIRED_KEY,  (bool)PMS_PAIRED_DEF);
  PutInt(V48_SCALE_NUM_KEY, (int)V48_SCALE_NUM_DEFAULT);
  PutInt(V48_SCALE_DEN_KEY, (int)V48_SCALE_DEN_DEFAULT);
  PutInt(VBAT_SCALE_NUM_KEY,(int)VBAT_SCALE_NUM_DEFAULT);
  PutInt(VBAT_SCALE_DEN_KEY,(int)VBAT_SCALE_DEN_DEFAULT);
  PutInt(VBUS_OVP_MV_KEY, (int)VBUS_OVP_MV_DEFAULT);
  PutInt(VBUS_UVP_MV_KEY, (int)VBUS_UVP_MV_DEFAULT);
  PutInt(IBUS_OCP_MA_KEY, (int)IBUS_OCP_MA_DEFAULT);
  PutInt(VBAT_OVP_MV_KEY, (int)VBAT_OVP_MV_DEFAULT);
  PutInt(VBAT_UVP_MV_KEY, (int)VBAT_UVP_MV_DEFAULT);
  PutInt(IBAT_OCP_MA_KEY, (int)IBAT_OCP_MA_DEFAULT);
  PutInt(OTP_C_KEY,       (int)OTP_C_DEFAULT);
  PutInt(PMS_TEL_MS_KEY,  (int)PMS_TEL_MS_DEFAULT);
  PutInt(PMS_REP_MS_KEY,  (int)PMS_REP_MS_DEFAULT);
  PutInt(PMS_HB_MS_KEY,   (int)PMS_HB_MS_DEFAULT);
  PutInt(PMS_SMOOTH_KEY,  (int)PMS_SMOOTH_DEFAULT);
  PutInt(PWR_WMIN_KEY,    (int)PWR_WMIN_DEF);
  PutInt(PWR_BMIN_KEY,    (int)PWR_BMIN_DEF);
  PutInt (FAN_ON_C_KEY,      (int)FAN_ON_C_DEFAULT);
  PutInt (FAN_OFF_C_KEY,     (int)FAN_OFF_C_DEFAULT);
  PutBool(BUZZER_ENABLE_KEY, (bool)BUZZER_ENABLE_DEFAULT);
  PutInt (BUZZER_VOLUME_KEY, (int)BUZZER_VOLUME_DEFAULT);
#endif
#if defined(NVS_ROLE_SENS)
  PutString(NVS_KEY_PRVMAC, NVS_DEF_PRVMAC);
  PutInt   (NVS_KEY_PRVTOK, (int)NVS_DEF_PRVTOK);
  PutString(NVS_KEY_NXTMAC, NVS_DEF_NXTMAC);
  PutInt   (NVS_KEY_NXTTOK, (int)NVS_DEF_NXTTOK);
  PutString(NVS_KEY_POSRLS, NVS_DEF_POSRLS);
  PutString(NVS_KEY_NEGRLS, NVS_DEF_NEGRLS);
  PutBool(SENS_PAIRING_KEY, (bool)SENS_PAIRING_DEF);
  PutBool(SENS_PAIRED_KEY,  (bool)SENS_PAIRED_DEF);
  PutInt(TF_NEAR_MM_KEY,    (int)TF_NEAR_MM_DEFAULT);
  PutInt(TF_FAR_MM_KEY,     (int)TF_FAR_MM_DEFAULT);
  PutInt(AB_SPACING_MM_KEY, (int)AB_SPACING_MM_DEFAULT);
  PutInt(ALS_T0_LUX_KEY,    (int)ALS_T0_LUX_DEFAULT);
  PutInt(ALS_T1_LUX_KEY,    (int)ALS_T1_LUX_DEFAULT);
  PutInt(CONFIRM_MS_KEY,    (int)CONFIRM_MS_DEFAULT);
  PutInt(STOP_MS_KEY,       (int)STOP_MS_DEFAULT);
  PutInt( RLY_ON_MS_KEY,   (int)RLY_ON_MS_DEFAULT);
  PutInt( RLY_OFF_MS_KEY,  (int)RLY_OFF_MS_DEFAULT);
  PutInt( LEAD_CNT_KEY,    (int)LEAD_CNT_DEFAULT);
  PutInt( LEAD_STP_MS_KEY, (int)LEAD_STP_MS_DEFAULT);
  PutInt(TFL_A_ADDR_KEY, TFL_ADDR_A);
  PutInt(TFL_B_ADDR_KEY, TFL_ADDR_B);
#endif
#if defined(NVS_ROLE_RELAY)
  PutString(NVS_KEY_SAMAC, NVS_DEF_SAMAC);
  PutInt   (NVS_KEY_SATOK, (int)NVS_DEF_SATOK);
  PutString(NVS_KEY_SBMAC, NVS_DEF_SBMAC);
  PutInt   (NVS_KEY_SBTOK, (int)NVS_DEF_SBTOK);
  PutInt   (NVS_KEY_SPLIT, (int)NVS_DEF_SPLIT);
  PutBool(REL_PAIRING_KEY, (bool)REL_PAIRING_DEF);
  PutBool(REL_PAIRED_KEY,  (bool)REL_PAIRED_DEF);
  PutInt (PULSE_MS_KEY,  (int)PULSE_MS_DEFAULT);
  PutInt (HOLD_MS_KEY,   (int)HOLD_MS_DEFAULT);
  PutBool(INTERLCK_KEY,  (bool)INTERLCK_DEFAULT);
  PutInt (RTLIM_C_KEY,   (int)RTLIM_C_DEFAULT);
#endif
#if defined(NVS_ROLE_SEMU)
  // --- Global/Device-level ---
  PutInt   (NVS_KEY_SCOUNT, (int)NVS_DEF_SCOUNT);

  // One global pairing state for the SEMU device
  PutBool  (SEMU_PAIRING_KEY, (bool)SEMU_PAIRING_DEF);
  PutBool  (SEMU_PAIRED_KEY,  (bool)SEMU_PAIRED_DEF);

  // Global neighbor links (prev/next) and relay labels
  PutString(NVS_KEY_PRVMAC,  NVS_DEF_PRVMAC);
  PutInt   (NVS_KEY_PRVTOK,  (int)NVS_DEF_PRVTOK);
  PutString(NVS_KEY_NXTMAC,  NVS_DEF_NXTMAC);
  PutInt   (NVS_KEY_NXTTOK,  (int)NVS_DEF_NXTTOK);
  PutString(NVS_KEY_POSRLS,  NVS_DEF_POSRLS);
  PutString(NVS_KEY_NEGRLS,  NVS_DEF_NEGRLS);

  // Virtual output behavior (global defaults)
  PutInt (VON_MS_KEY,    (int)VON_MS_DEF);
  PutInt (VLEAD_CT_KEY,  (int)VLEAD_CT_DEF);
  PutInt (VLEAD_MS_KEY,  (int)VLEAD_MS_DEF);

  // One ambient light sensor shared by all pairs
  PutInt (ALS_T0_LUX_KEY, (int)ALS_T0_LUX_DEFAULT);
  PutInt (ALS_T1_LUX_KEY, (int)ALS_T1_LUX_DEFAULT);

  // Emit ENV model per virtual sensor (0/1)
  PutBool(VENV_EN_KEY, (bool)VENV_EN_DEF);

  // --- Per-pair (index 0..count-1) defaults ---
  {
    const int count = GetInt(NVS_KEY_SCOUNT, (int)NVS_DEF_SCOUNT);
    // Safer buffer; prefixes are 6 chars, plus up to 2 digits and NUL
    auto put_u16_by_pfx = [&](const char* pfx, int idx, uint16_t val) {
      char key[12]; snprintf(key, sizeof(key), "%s%d", pfx, idx);
      PutInt(key, (int)val);
    };
    auto put_u8_by_pfx = [&](const char* pfx, int idx, uint8_t val) {
      char key[12]; snprintf(key, sizeof(key), "%s%d", pfx, idx);
      PutInt(key, (int)val);
    };

    for (int i = 0; i < count; ++i) {
      // Per-pair near/far thresholds (mm)
      put_u16_by_pfx(TF_NEAR_MM_KEY_PFX, i, (uint16_t)TF_NEAR_MM_DEFAULT);
      put_u16_by_pfx(TF_FAR_MM_KEY_PFX,  i, (uint16_t)TF_FAR_MM_DEFAULT);

      // Per-pair A↔B spacing (mm)
      put_u16_by_pfx(AB_SPACING_MM_KEY_PFX, i, (uint16_t)AB_SPACING_MM_DEFAULT);

      // Per-pair TF-Luna I2C addresses
      put_u8_by_pfx (TFL_A_ADDR_KEY_PFX, i, (uint8_t)TFL_ADDR_A_DEF);
      put_u8_by_pfx (TFL_B_ADDR_KEY_PFX, i, (uint8_t)TFL_ADDR_B_DEF);

      // Per-pair frame rate (FPS)
      put_u16_by_pfx(TFL_FPS_KEY_PFX, i, (uint16_t)TFL_FPS_DEF);
    }

    // Keep your existing per-virtual security token + neighbor MAC/TOK init (1-based)
    const uint64_t ef   = ESP.getEfuseMac();
    const uint32_t seed = (uint32_t)(ef ^ (ef >> 23) ^ 0xA5A5A5A5UL);
    for (int i = 1; i <= count; ++i) {
      // Virtual token
      char vtkey[7]; vtkey[6] = '\0';
      snprintf(vtkey, sizeof(vtkey), NVS_SEMU_VTOK_FMT, (unsigned)i);
      uint32_t vtok = (seed ^ (i * 2654435761UL)) & 0xFFFFu; if (vtok == 0) vtok = 1;
      PutInt(vtkey, (int)vtok);

      // Prev link
      char pM[7], pT[7]; pM[6]=pT[6]='\0';
      snprintf(pM, sizeof(pM), NVS_SEMU_PMAC_FMT, (unsigned)i);
      snprintf(pT, sizeof(pT), NVS_SEMU_PTOK_FMT, (unsigned)i);
      PutString(pM, NVS_DEF_MAC_EMPTY);
      PutInt   (pT, 0);

      // Next link
      char nM[7], nT[7]; nM[6]=nT[6]='\0';
      snprintf(nM, sizeof(nM), NVS_SEMU_NMAC_FMT, (unsigned)i);
      snprintf(nT, sizeof(nT), NVS_SEMU_NTOK_FMT, (unsigned)i);
      PutString(nM, NVS_DEF_MAC_EMPTY);
      PutInt   (nT, 0);
    }
  }
#endif

#if defined(NVS_ROLE_REMU)
  // Size of the emulator (defaults to 16)
  PutInt(NVS_KEY_RCOUNT, (int)NVS_DEF_RCOUNT);             // e.g., 16

  // Optional device-level boundary mapping defaults (shared)
  PutString(NVS_KEY_SAMAC, NVS_DEF_SAMAC);
  PutInt   (NVS_KEY_SATOK, (int)NVS_DEF_SATOK);
  PutString(NVS_KEY_SBMAC, NVS_DEF_SBMAC);
  PutInt   (NVS_KEY_SBTOK, (int)NVS_DEF_SBTOK);
  PutInt   (NVS_KEY_SPLIT, (int)NVS_DEF_SPLIT);

  // Global relay behavior defaults (device-level)
  PutInt (RPULSE_MS_KEY, (int)RPULSE_MS_DEF);              // default ON ms
  PutInt (RHOLD_MS_KEY,  (int)RHOLD_MS_DEF);               // default cap ms
  PutInt (RREP_MS_KEY,   (int)RREP_MS_DEF);                // report cadence
  PutString(RILOCK_JS_KEY, RILOCK_JS_DEF);                 // interlock groups (JSON)

  // ---------- Per-virtual setup (index 0..count-1) ----------
  {
    const int count = GetInt(NVS_KEY_RCOUNT, (int)NVS_DEF_RCOUNT);

    // Small helpers: write prefixed u16 (prefix + index)
    auto put_u16 = [&](const char* pfx, int idx, uint16_t val){
      char key[12]; snprintf(key, sizeof(key), "%s%d", pfx, idx);
      PutInt(key, (int)val);
    };

    // Per-relay defaults (override keys)
    for (int i = 0; i < count; ++i) {
      put_u16(RPULSE_MS_PFX, i, (uint16_t)RPULSE_MS_DEF);  // per-output pulse
      put_u16(RHOLD_MS_PFX,  i, (uint16_t)RHOLD_MS_DEF);   // per-output hold cap
    }

    // Security tokens & per-virtual A/B boundary mapping
    // Formats are defined centrally in NVSConfig.h:
    //   O%02uTOK, A%02uMAC/A%02uTOK, B%02uMAC/B%02uTOK
    const uint64_t ef   = ESP.getEfuseMac();
    const uint32_t seed = (uint32_t)((ef >> 16) ^ ef ^ 0x5C5C3C3CUL);

    for (int i = 1; i <= count; ++i) {
      // Virtual relay token (OxxTOK)
      char ok[7]; ok[6] = '\0';
      snprintf(ok, sizeof(ok), NVS_REMU_OTOK_FMT, (unsigned)i);
      uint32_t otok = ((seed << 1) ^ (i * 1140071485UL)) & 0xFFFFu;
      if (otok == 0) otok = 1;
      PutInt(ok, (int)otok);

      // Boundary sensor A (AxxMAC/AxxTOK)
      char aM[7], aT[7]; aM[6]=aT[6]='\0';
      snprintf(aM, sizeof(aM), NVS_REMU_AMAC_FMT, (unsigned)i);
      snprintf(aT, sizeof(aT), NVS_REMU_ATOK_FMT, (unsigned)i);
      PutString(aM, NVS_DEF_MAC_EMPTY);
      PutInt   (aT, 0);

      // Boundary sensor B (BxxMAC/BxxTOK)
      char bM[7], bT[7]; bM[6]=bT[6]='\0';
      snprintf(bM, sizeof(bM), NVS_REMU_BMAC_FMT, (unsigned)i);
      snprintf(bT, sizeof(bT), NVS_REMU_BTOK_FMT, (unsigned)i);
      PutString(bM, NVS_DEF_MAC_EMPTY);
      PutInt   (bT, 0);
    }
  }
#endif

  PutBool(RESET_FLAG_KEY, false);
}
bool NvsManager::GetBool(const char* key, bool defaultValue) {
    esp_task_wdt_reset();
    bool value = pref.getBool(key, defaultValue);
    return value;
}
int NvsManager::GetInt(const char* key, int defaultValue) {
    esp_task_wdt_reset();
    int value = pref.getInt(key, defaultValue);
    return value;
}
uint64_t NvsManager::GetULong64(const char* key, int defaultValue) {
    esp_task_wdt_reset();
    uint64_t value = pref.getULong64(key, defaultValue);
    return value;
}
float NvsManager::GetFloat(const char* key, float defaultValue) {
    esp_task_wdt_reset();
    float value = pref.getFloat(key, defaultValue);
    return value;
}
String NvsManager::GetString(const char* key, const String& defaultValue) {
    esp_task_wdt_reset();
    String value = pref.getString(key, defaultValue);
    return value;
}
void NvsManager::PutBool(const char* key, bool value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putBool(key, value);
}
void NvsManager::PutUInt(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putUInt(key, value);
}
void NvsManager::PutULong64(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putULong64(key, value);
}
void NvsManager::PutInt(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putInt(key, value);
}
void NvsManager::PutFloat(const char* key, float value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putFloat(key, value);
}
void NvsManager::PutString(const char* key, const String& value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    pref.putString(key, value);
}
void NvsManager::ClearKey() {
    pref.clear();
}
bool NvsManager::Iskey(const char* key){
   return  pref.isKey(key);
}
void NvsManager::RemoveKey(const char * key) {
    esp_task_wdt_reset();
    if (pref.isKey(key)) {
        pref.remove(key);
        if (DEBUGMODE) {
            DEBUG_PRINT("Removed key: ");
            DEBUG_PRINTLN(key);
        }
    } else if (DEBUGMODE) {
        DEBUG_PRINT("Key not found, skipping: ");
        DEBUG_PRINTLN(key);
    }
}

