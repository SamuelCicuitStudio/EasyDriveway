
#include "ConfigManager.h"



ConfigManager::ConfigManager(Preferences* preferences) : preferences(preferences),namespaceName(CONFIG_PARTITION) {}

ConfigManager::~ConfigManager() {
    end();  // Ensure preferences are closed properly
}

void ConfigManager::RestartSysDelayDown(unsigned long delayTime) {
    unsigned long startTime = millis();  // Record the start time

    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Restarting the Device in: ");
        DEBUG_PRINT(delayTime / 1000);  // Convert delayTime to seconds
        DEBUG_PRINTLN(" Sec");
    }

    // Ensure 32 '#' are printed after the countdown
    unsigned long interval = delayTime / 32;  // Divide delayTime by 32 to get interval

    if (DEBUGMODE) {
        for (int i = 0; i < 32; i++) {  // Print 32 '#' characters
            DEBUG_PRINT("#");
            delay(interval);  // Delay for visibility of each '#' character
            esp_task_wdt_reset();  // Reset watchdog timer
        }
        DEBUG_PRINTLN();  // Move to the next line after printing
    }

    if (DEBUGMODE) {
        DEBUG_PRINTLN("Restarting now...");
    }
    simulatePowerDown();  // Simulate power down before restart
    
}

void ConfigManager::RestartSysDelay(unsigned long delayTime) {
    unsigned long startTime = millis();  // Record the start time

    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Restarting the Device in: ");
        DEBUG_PRINT(delayTime / 1000);  // Convert delayTime to seconds
        DEBUG_PRINTLN(" Sec");
    }

    // Ensure 32 '#' are printed after the countdown
    unsigned long interval = delayTime / 32;  // Divide delayTime by 32 to get interval

    if (DEBUGMODE) {
        for (int i = 0; i < 32; i++) {  // Print 32 '#' characters
            DEBUG_PRINT("#");
            delay(interval);  // Delay for visibility of each '#' character
            esp_task_wdt_reset();  // Reset watchdog timer
        }
        DEBUG_PRINTLN();  // Move to the next line after printing
    }

    if (DEBUGMODE) {
        DEBUG_PRINTLN("Restarting now...");
    }
    //simulatePowerDown();  // Simulate power down before restart
     ESP.restart();
}

void ConfigManager::CountdownDelay(unsigned long delayTime) {
    unsigned long startTime = millis();  // Record the start time

    if (DEBUGMODE) {
        DEBUG_PRINTLN("################################");
        DEBUG_PRINT("Waiting User Action: ");
        DEBUG_PRINT(delayTime / 1000);  // Convert delayTime to seconds
        DEBUG_PRINTLN(" Sec");
    }

    // Ensure 32 '#' are printed after the countdown
    if (DEBUGMODE) {
        unsigned long interval = delayTime / 32;  // Divide delayTime by 32 to get interval

        for (int i = 0; i < 32; i++) {  // Print 32 '#' characters
            DEBUG_PRINT("#");
            delay(interval);  // Delay dynamically based on the given delayTime
            esp_task_wdt_reset();  // Reset watchdog timer
        }
        DEBUG_PRINTLN();  // Move to the next line after printing
    }
}

void ConfigManager::simulatePowerDown() {
    // Put the ESP32 into deep sleep for 1 second (simulate power-down)
    esp_sleep_enable_timer_wakeup(1000000); // 1 second (in microseconds)
    esp_deep_sleep_start();  // Enter deep sleep
}

void ConfigManager::startPreferencesReadWrite() {
    preferences->begin(CONFIG_PARTITION, false);  // false = read-write mode
    DEBUG_PRINTLN("Preferences opened in write mode.");
}

void ConfigManager::startPreferencesRead() {
    preferences->begin(CONFIG_PARTITION, true);  // true = read-only mode
    DEBUG_PRINTLN("Preferences opened in read mode.");
}

void ConfigManager::begin() {

    DEBUG_PRINTLN("###########################################################");
    DEBUG_PRINTLN("#               Starting CONFIG Manager                   #");
    DEBUG_PRINTLN("###########################################################");
  
    bool resetFlag = GetBool(RESET_FLAG_KEY, true);  // Default to true if not set

    if (resetFlag) {
        // Only print once, if necessary, then reset device
        DEBUG_PRINTLN("ConfigManager: Initializing the device...");
        initializeDefaults();  // Reset preferences if the flag is set
        RestartSysDelay(7000);  // Use a delay for restart after reset
    } else {
        // Use existing configuration, no need for unnecessary delay
        DEBUG_PRINTLN("ConfigManager: Using existing configuration...");
    }
}

bool ConfigManager::getResetFlag() {
    esp_task_wdt_reset();
    bool value = preferences->getBool(RESET_FLAG_KEY, true); // Default to true if not set
    return value;
}

void ConfigManager::end() {
    preferences->end();  // Close preferences
}

void ConfigManager::initializeDefaults() {
    initializeVariables();  // Initialize all default variables
}

// Format a 48-bit MAC as 12 uppercase hex chars (no colons)
static String mac12_from_efuse() {
  uint64_t m = ESP.getEfuseMac();  // 0xAABBCCDDEEFF (LSB-first on ESP32)
  char buf[13];
  // Normalize to the human-expected order (AA..FF). Shift bytes accordingly.
  uint8_t bytes[6] = {
    (uint8_t)(m >> 40), (uint8_t)(m >> 32), (uint8_t)(m >> 24),
    (uint8_t)(m >> 16), (uint8_t)(m >> 8 ), (uint8_t)(m >> 0 )
  };
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
           bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5]);
  return String(buf);  // e.g. "246F28A1B2C3"
}

void ConfigManager::initializeVariables() {
  // ----------- Unique identity (independent of role) -----------
  const String mac12   = mac12_from_efuse();        // "246F28A1B2C3"
  const String macTail = mac12.substring(6);        // "A1B2C3"
  const String uniqId  = String("DL-") + mac12;     // e.g. "DL-246F28A1B2C3"
  const String uniqNm  = String("DL_") + macTail;   // e.g. "DL_A1B2C3"

  // SYS
  PutInt   (NVS_KEY_SYS_KIND,  NVS_DEF_SYS_KIND);
  PutString(NVS_KEY_SYS_DEVID, uniqId.c_str());     // <-- unique, role-agnostic
  PutString(NVS_KEY_SYS_HWREV, NVS_DEF_SYS_HWREV);
  PutString(NVS_KEY_SYS_SWVER, NVS_DEF_SYS_SWVER);
  PutString(NVS_KEY_SYS_BUILD, NVS_DEF_SYS_BUILD);
  PutString(NVS_KEY_SYS_DEFNM, uniqNm.c_str());     // <-- unique, role-agnostic
  PutBool  (RESET_FLAG_KEY, false);
  PutInt(NVS_KEY_MODE, (int)NVS_DEF_MODE);             // 0=AUTO, 1=MAN

  // Link & admission
  PutInt   (NVS_KEY_NET_CHAN,   NVS_DEF_NET_CHAN);
  PutString(NVS_KEY_NET_ICMMAC, NVS_DEF_NET_ICMMAC);
  PutInt   (NVS_KEY_NET_PAIRED, NVS_DEF_NET_PAIRED);
  PutInt   (NVS_KEY_ESP_TOKEN,  NVS_DEF_ESP_TOKEN);

  // Permanent indicator policy
  PutInt(NVS_KEY_IND_LEDDIS, NVS_DEF_IND_LEDDIS);
  PutInt(NVS_KEY_IND_BUZDIS, NVS_DEF_IND_BUZDIS);

  // ======================= ICM ============================
#if defined(NVS_ROLE_ICM)

   PutBool(GOTO_CONFIG_KEY,    RESET_FLAG_DEFAULT);          // false
   PutString(WEB_USER_KEY, WEB_USER_DEFAULT);
   PutString(WEB_PASS_KEY, WEB_PASS_DEFAULT);
   PutString(PASS_PIN_KEY, PASS_PIN_DEFAULT);
  // BLE name / AP SSID use the same unique device name
  PutString(NVS_KEY_BLE_NAME,      uniqNm.c_str());
  PutString(NVS_KEY_WIFI_APSSID,   uniqNm.c_str());
  PutString(NVS_KEY_WIFI_APKEY,    NVS_DEF_WIFI_APKEY);
  PutString(NVS_KEY_WIFI_STSSID,   NVS_DEF_WIFI_STSSID);
  PutString(NVS_KEY_WIFI_STKEY,    NVS_DEF_WIFI_STKEY);

  // Deterministic 6-digit BLE passkey + 6-digit pairing PIN
  // Mix the eFuse MAC to generate two distinct codes in [100000..999999]
  uint64_t ef = ESP.getEfuseMac();
  uint32_t mix = (uint32_t)(ef ^ (ef >> 21) ^ (ef >> 33));
  uint32_t rot = (mix << 7) | (mix >> (32 - 7));
  uint32_t blePass = ((mix * 2654435761UL) % 900000) + 100000;
  uint32_t pin6    = ((rot ^ 0x5A5A5A5AUL)   % 900000) + 100000;

  PutInt(NVS_KEY_BLE_PASSK, (int)blePass);  // BLE passkey
  PutInt(NVS_KEY_AUTH_PIN,  (int)pin6);     // 6-digit PIN

  // Topology & registry
  PutString(NVS_KEY_TOPO_STRING, NVS_DEF_TOPO_STRING);
  PutString(NVS_KEY_REG_SLMACS,  NVS_DEF_REG_SLMACS);

  // Pins (ICM_PinsNVS.h)
  PutInt(NVS_PIN_SD_MISO, PIN_SD_MISO_DEFAULT);
  PutInt(NVS_PIN_SD_MOSI, PIN_SD_MOSI_DEFAULT);
  PutInt(NVS_PIN_SD_CS,   PIN_SD_CS_DEFAULT);
  PutInt(NVS_PIN_SD_SCK,  PIN_SD_SCK_DEFAULT);
  PutString(NVS_SD_CARD_MODEL_KEY, SD_CARD_MODEL_DEFAULT); // "MKDV8GIL-AST"
  // Boot/CLI mode:
  PutBool(SERIAL_ONLY_FLAG_KEY, SERIAL_ONLY_FLAG_DEFAULT);

  PutInt(NVS_PIN_PMS_RX,  PIN_PMS_RX_DEFAULT);
  PutInt(NVS_PIN_PMS_TX,  PIN_PMS_TX_DEFAULT);
  PutInt(NVS_PIN_PMS_BAUD,PMS_BAUD_DEFAULT);

  PutInt(NVS_PIN_FAN_PWM, PIN_FAN_PWM_DEFAULT);
  PutInt(NVS_PIN_LED_R,   PIN_LED_R_DEFAULT);
  PutInt(NVS_PIN_LED_G,   PIN_LED_G_DEFAULT);
  PutInt(NVS_PIN_LED_B,   PIN_LED_B_DEFAULT);

  PutInt(NVS_PIN_RTC_INT, PIN_RTC_INT_DEFAULT);
  PutInt(NVS_PIN_RTC_32K, PIN_RTC_32K_DEFAULT);
  PutInt(NVS_PIN_I2C_SCL, PIN_I2C_SCL_DEFAULT);
  PutInt(NVS_PIN_I2C_SDA, PIN_I2C_SDA_DEFAULT);
  PutInt(NVS_PIN_RTC_RST, PIN_RTC_RST_DEFAULT);
  PutString(NVS_RTC_MODEL_KEY, RTC_MODEL_DEFAULT);

  PutInt(NVS_PIN_TS_GPIO, PIN_TS_GPIO_DEFAULT);
  PutBool(TEMP_SENSOR_PULLUP_KEY, TEMP_SENSOR_PULLUP_DEFAULT);
  PutInt(NVS_PIN_BUZ_GPIO,PIN_BUZ_GPIO_DEFAULT);
  PutInt(NVS_PIN_BUZ_AH,  BUZ_AH_DEFAULT);
  PutInt(NVS_PIN_BUZ_FBK, BUZ_FBK_DEFAULT);
#endif

  // ======================= PMS ============================
#if defined(NVS_ROLE_PMS)
  PutInt(NVS_PIN_SD_MISO, PIN_SD_MISO_DEFAULT);
  PutInt(NVS_PIN_SD_MOSI, PIN_SD_MOSI_DEFAULT);
  PutInt(NVS_PIN_SD_CS,   PIN_SD_CS_DEFAULT);
  PutInt(NVS_PIN_SD_SCK,  PIN_SD_SCK_DEFAULT);
  PutString(NVS_SD_CARD_MODEL_KEY, SD_CARD_MODEL_DEFAULT); // "MKDV8GIL-AST"

  PutInt(NVS_PIN_I2C_SCL, PIN_I2C_SCL_DEFAULT);
  PutInt(NVS_PIN_I2C_SDA, PIN_I2C_SDA_DEFAULT);

  PutInt(NVS_PIN_FAN_PWM, PIN_FAN_PWM_DEFAULT);
  PutInt(NVS_PIN_LED_R,   PIN_LED_R_DEFAULT);
  PutInt(NVS_PIN_LED_G,   PIN_LED_G_DEFAULT);
  PutInt(NVS_PIN_LED_B,   PIN_LED_B_DEFAULT);

  PutInt(NVS_PIN_BUZ_GPIO, PIN_BUZ_GPIO_DEFAULT);
  PutInt(NVS_PIN_BUZ_AH,   BUZ_AH_DEFAULT);
  PutInt(NVS_PIN_BUZ_FBK,  BUZ_FBK_DEFAULT);

  PutInt(NVS_PIN_TS_GPIO,  PIN_TS_GPIO_DEFAULT);
  PutBool(TEMP_SENSOR_PULLUP_KEY, TEMP_SENSOR_PULLUP_DEFAULT);

  PutBool(PMS_PAIRING_KEY, PMS_PAIRING_DEF);
  PutBool(PMS_PAIRED_KEY, PMS_PAIRED_DEF);

  PutInt(NVS_PIN_P48_EN,   PIN_P48_EN_DEFAULT);
  PutInt(NVS_PIN_P5V_EN,   PIN_P5V_EN_DEFAULT);
  PutInt(NVS_PIN_MAINS_OK, PIN_MAINS_OK_DEFAULT);
  PutInt(NVS_PIN_V48_ADC,  PIN_V48_ADC_DEFAULT);
  PutInt(NVS_PIN_VBAT_ADC, PIN_VBAT_ADC_DEFAULT);
  PutInt(NVS_PIN_I48_ADC,  PIN_I48_ADC_DEFAULT);
  PutInt(NVS_PIN_IBAT_ADC, PIN_IBAT_ADC_DEFAULT);
  PutInt(NVS_PIN_CH_EN,    PIN_CH_EN_DEFAULT);

  PutInt(NVS_PIN_BOOT,   PIN_BOOT_DEFAULT);
  PutInt(NVS_PIN_PWRBTN, PIN_PWRBTN_DEFAULT);
    // Boot/CLI mode:
  PutBool(SERIAL_ONLY_FLAG_KEY, SERIAL_ONLY_FLAG_DEFAULT);

  // Measurement scaling:
  PutInt(V48_SCALE_NUM_KEY,  V48_SCALE_NUM_DEFAULT);
  PutInt(V48_SCALE_DEN_KEY,  V48_SCALE_DEN_DEFAULT);
  PutInt(VBAT_SCALE_NUM_KEY, VBAT_SCALE_NUM_DEFAULT);
  PutInt(VBAT_SCALE_DEN_KEY, VBAT_SCALE_DEN_DEFAULT);

  // Fault thresholds:
  PutInt(VBUS_OVP_MV_KEY, VBUS_OVP_MV_DEFAULT);
  PutInt(VBUS_UVP_MV_KEY, VBUS_UVP_MV_DEFAULT);
  PutInt(IBUS_OCP_MA_KEY, IBUS_OCP_MA_DEFAULT);
  PutInt(VBAT_OVP_MV_KEY, VBAT_OVP_MV_DEFAULT);
  PutInt(VBAT_UVP_MV_KEY, VBAT_UVP_MV_DEFAULT);
  PutInt(IBAT_OCP_MA_KEY, IBAT_OCP_MA_DEFAULT);
  PutInt(OTP_C_KEY,       OTP_C_DEFAULT);

  // PMS telemetry/reporting cadence & smoothing:
  PutInt(PMS_TEL_MS_KEY, PMS_TEL_MS_DEFAULT);
  PutInt(PMS_REP_MS_KEY, PMS_REP_MS_DEFAULT);
  PutInt(PMS_HB_MS_KEY,  PMS_HB_MS_DEFAULT);
  PutInt(PMS_SMOOTH_KEY, PMS_SMOOTH_DEFAULT);
#endif

  // ======================= SENSOR =========================
#if defined(NVS_ROLE_SENS)
  PutString(NVS_KEY_TOPO_PRVMAC, NVS_DEF_TOPO_PRVMAC);
  PutInt   (NVS_KEY_TOPO_PRVTOK, NVS_DEF_TOPO_PRVTOK);
  PutString(NVS_KEY_TOPO_NXTMAC, NVS_DEF_TOPO_NXTMAC);
  PutInt   (NVS_KEY_TOPO_NXTTOK, NVS_DEF_TOPO_NXTTOK);
  PutString(NVS_KEY_TOPO_POSRLS, NVS_DEF_TOPO_POSRLS);
  PutString(NVS_KEY_TOPO_NEGRLS, NVS_DEF_TOPO_NEGRLS);
  PutInt   (NVS_KEY_TOPO_ROLE,   NVS_DEF_TOPO_ROLE);

  PutInt(NVS_PIN_SD_MISO, PIN_SD_MISO_DEFAULT);
  PutInt(NVS_PIN_SD_MOSI, PIN_SD_MOSI_DEFAULT);
  PutInt(NVS_PIN_SD_CS,   PIN_SD_CS_DEFAULT);
  PutInt(NVS_PIN_SD_SCK,  PIN_SD_SCK_DEFAULT);
  PutString(NVS_SD_CARD_MODEL_KEY, SD_CARD_MODEL_DEFAULT); // "MKDV8GIL-AST"

  PutInt(NVS_PIN_FAN_PWM, PIN_FAN_PWM_DEFAULT);
  PutInt(NVS_PIN_LED_R,   PIN_LED_R_DEFAULT);
  PutInt(NVS_PIN_LED_G,   PIN_LED_G_DEFAULT);
  PutInt(NVS_PIN_LED_B,   PIN_LED_B_DEFAULT);
  
  PutInt(NVS_PIN_BUZ_GPIO,PIN_BUZ_GPIO_DEFAULT);
  PutInt(NVS_PIN_BUZ_AH,   BUZ_AH_DEFAULT);
  PutInt(NVS_PIN_BUZ_FBK,  BUZ_FBK_DEFAULT);

  PutBool(SENS_PAIRING_KEY, SENS_PAIRING_DEF);
  PutBool(SENS_PAIRED_KEY, SENS_PAIRED_DEF);

  PutInt(NVS_PIN_BOOT,    PIN_BOOT_DEFAULT);

  PutInt(NVS_PIN_I2C1_SDA, PIN_I2C1_SDA_DEFAULT);
  PutInt(NVS_PIN_I2C1_SCL, PIN_I2C1_SCL_DEFAULT);
  PutInt(NVS_PIN_I2C2_SDA, PIN_I2C2_SDA_DEFAULT);
  PutInt(NVS_PIN_I2C2_SCL, PIN_I2C2_SCL_DEFAULT);
#endif

  // ======================= RELAY ==========================
#if defined(NVS_ROLE_RELAY)
  PutString(NVS_KEY_BND_SAMAC, NVS_DEF_BND_SAMAC);
  PutInt   (NVS_KEY_BND_SATOK, NVS_DEF_BND_SATOK);
  PutString(NVS_KEY_BND_SBMAC, NVS_DEF_BND_SBMAC);
  PutInt   (NVS_KEY_BND_SBTOK, NVS_DEF_BND_SBTOK);
  PutInt   (NVS_KEY_BND_SPLIT, NVS_DEF_BND_SPLIT);

  PutInt(NVS_PIN_SD_MISO, PIN_SD_MISO_DEFAULT);
  PutInt(NVS_PIN_SD_MOSI, PIN_SD_MOSI_DEFAULT);
  PutInt(NVS_PIN_SD_CS,   PIN_SD_CS_DEFAULT);
  PutInt(NVS_PIN_SD_SCK,  PIN_SD_SCK_DEFAULT);
  PutString(NVS_SD_CARD_MODEL_KEY, SD_CARD_MODEL_DEFAULT); // "MKDV8GIL-AST"

  PutInt(NVS_PIN_RELAY_LEFT,  PIN_LEFT_DEFAULT);
  PutInt(NVS_PIN_RELAY_RIGHT, PIN_RIGHT_DEFAULT);

  PutBool(REL_PAIRING_KEY, REL_PAIRING_DEF);
  PutBool(REL_PAIRED_KEY, REL_PAIRED_DEF);

  PutInt(NVS_PIN_LED_R,   PIN_LED_R_DEFAULT);
  PutInt(NVS_PIN_LED_G,   PIN_LED_G_DEFAULT);
  PutInt(NVS_PIN_LED_B,   PIN_LED_B_DEFAULT);

  PutInt(NVS_PIN_BUZ_GPIO, PIN_BUZ_GPIO_DEFAULT);
  PutInt(NVS_PIN_BUZ_AH,   BUZ_AH_DEFAULT);
  PutInt(NVS_PIN_BUZ_FBK,  BUZ_FBK_DEFAULT);
  PutInt(NVS_PIN_TS_GPIO,  PIN_TS_GPIO_DEFAULT);
  PutBool(TEMP_SENSOR_PULLUP_KEY, TEMP_SENSOR_PULLUP_DEFAULT);
  PutInt(NVS_PIN_BOOT,     PIN_BOOT_DEFAULT);
#endif
}

bool ConfigManager::GetBool(const char* key, bool defaultValue) {
    esp_task_wdt_reset();
    bool value = preferences->getBool(key, defaultValue);
    return value;
}

int ConfigManager::GetInt(const char* key, int defaultValue) {
    esp_task_wdt_reset();
    int value = preferences->getInt(key, defaultValue);
    return value;
}

uint64_t ConfigManager::GetULong64(const char* key, int defaultValue) {
    esp_task_wdt_reset();
    uint64_t value = preferences->getULong64(key, defaultValue);
    return value;
}

float ConfigManager::GetFloat(const char* key, float defaultValue) {
    esp_task_wdt_reset();
    float value = preferences->getFloat(key, defaultValue);
    return value;
}

String ConfigManager::GetString(const char* key, const String& defaultValue) {
    esp_task_wdt_reset();
    String value = preferences->getString(key, defaultValue);
    return value;
}

void ConfigManager::PutBool(const char* key, bool value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putBool(key, value);  // Store the new value
}

void ConfigManager::PutUInt(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putUInt(key, value);  // Store the new value
}

void ConfigManager::PutULong64(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putULong64(key, value);  // Store the new value
}

void ConfigManager::PutInt(const char* key, int value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putInt(key, value);  // Store the new value
}

void ConfigManager::PutFloat(const char* key, float value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putFloat(key, value);  // Store the new value
}

void ConfigManager::PutString(const char* key, const String& value) {
    esp_task_wdt_reset();
    RemoveKey(key);
    preferences->putString(key, value);  // Store the new value
}

void ConfigManager::ClearKey() {
    preferences->clear();
}

bool ConfigManager::Iskey(const char* key){
   return  preferences->isKey(key);

};

void ConfigManager::RemoveKey(const char * key) {
    esp_task_wdt_reset();  // Reset the watchdog timer

    // Check if the key exists before removing it
    if (preferences->isKey(key)) {
        preferences->remove(key);  // Remove the key if it exists
        if (DEBUGMODE) {
            DEBUG_PRINT("Removed key: ");
            DEBUG_PRINTLN(key);
        }
    } else if (DEBUGMODE) {
        DEBUG_PRINT("Key not found, skipping: ");
        DEBUG_PRINTLN(key);
    }
}


