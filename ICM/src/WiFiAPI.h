/**************************************************************
 *  Project  : ICM (Interface Control Module)
 *  Headers  : WiFiAPI.h  
 *  Note     : Readability regrouping only — no API changes.
 *             All macros, types, methods preserved verbatim.
 **************************************************************/

// ============================== WiFiAPI.h ==============================
#ifndef WIFI_API_H
#define WIFI_API_H

// ---------------- Pages ----------------
#define API_ROOT            "/"
#define PAGE_LOGIN          "/login.html"
#define PAGE_LOGIN_FAIL     "/login_failed.html"
#define PAGE_HOME           "/home.html"
#define PAGE_SETTINGS       "/settings.html"
#define PAGE_WIFI           "/wifi.html"
#define PAGE_TOPO           "/topology.html"
#define PAGE_LIVE           "/live.html"
#define PAGE_THANKYOU       "/thankyou.html"

// ---------------- Auth ----------------
#define API_LOGIN_ENDPOINT  "/connect"          // POST {username,password} (also accepts user/login, pass)
#define API_LOGOUT_ENDPOINT "/logout"           // POST clear cookie
#define API_AUTH_STATUS     "/api/auth/status"  // GET -> {ok:true/false}

// ---------------- System mode ----------------
#define API_SYS_MODE        "/api/system/mode"  // POST {mode:"AUTO|MANUAL"} or {manual:true|false}
#define API_ESPNOW_MODE     "/api/espnow/mode"  // alias; same handler

// ---------------- Configuration ----------------
#define API_CFG_LOAD          "/api/config/load"            // GET
#define API_CFG_SAVE          "/api/config/save"            // POST {ap_ssid,ap_password,ble_name,ble_password,esn_ch[,ssid,password]}
#define API_CFG_EXPORT        "/api/config/export"          // GET -> { export: "<blob>" }
#define API_CFG_IMPORT        "/api/config/import"          // POST { config: {...} }
#define API_CFG_FACTORY_RESET "/api/config/factory_reset"   // POST

// ---------------- Wi‑Fi control ----------------
#define API_WIFI_MODE           "/api/wifi/mode"            // GET  -> {mode:"AP|STA|OFF", ip?, ch?, rssi?}
#define API_WIFI_STA_CONNECT    "/api/wifi/sta/connect"     // POST {ssid,password}
#define API_WIFI_STA_DISCONNECT "/api/wifi/sta/disconnect"  // POST
#define API_WIFI_AP_START       "/api/wifi/ap/start"        // POST {ap_ssid,ap_password,esn_ch?}
#define API_WIFI_SCAN           "/api/wifi/scan"            // GET  -> {aps:[{ssid,rssi,ch,enc},...]}

// ---------------- ESP‑NOW peers / topology ----------------
#define API_PEERS_LIST       "/api/peers/list"   // GET
#define API_PEER_PAIR        "/api/peer/pair"    // POST {mac,type}
#define API_PEER_REMOVE      "/api/peer/remove"  // POST {mac}
#define API_TOPOLOGY_SET     "/api/topology/set" // POST {links:[...]}
#define API_TOPOLOGY_GET     "/api/topology/get" // GET

// ---------------- Sensors ----------------
#define API_SENSOR_DAYNIGHT  "/api/sensor/daynight" // POST {mac} -> {ok,is_day,updated_ms}
// NEW: one-shot environment (BME280 + ALS)
#define API_SENSOR_ENV       "/api/sensor/env"      // POST {mac}
                                                    // -> { ok:true,
                                                    //      tC:float, rh:float, p_Pa:int, lux:float,
                                                    //      is_day:0|1 }

// NEW: one-shot TF-Luna raw snapshot (both A & B)
#define API_SENSOR_TFRAW     "/api/sensor/tfraw"    // POST {mac, which?:0|1|2}
                                                    // -> { ok:true,
                                                    //      distA_mm:uint, ampA:uint, okA:0|1,
                                                    //      distB_mm:uint, ampB:uint, okB:0|1 }
// ---------------- Sequences ----------------
#define API_SEQUENCE_START   "/api/sequence/start" // POST {start, direction:"UP|DOWN"}
#define API_SEQUENCE_STOP    "/api/sequence/stop"  // POST

// ---------------- System / quick controls ----------------
#define API_SYS_STATUS   "/api/system/status"   // GET
#define API_BUZZ_SET     "/api/buzzer/set"      // POST {enabled:bool}
#define API_SYS_RESET    "/api/system/reset"    // POST
#define API_SYS_RESTART  "/api/system/restart"  // POST

// ---------------- Cooling ----------------
#define API_COOL_STATUS   "/api/cooling/status"  // GET
#define API_COOL_MODE     "/api/cooling/mode"    // POST {mode:"AUTO|ECO|NORMAL|FORCED|STOPPED"}
#define API_COOL_SPEED    "/api/cooling/speed"   // POST {pct:0..100}

// ---------------- Sleep ----------------
#define API_SLEEP_STATUS  "/api/sleep/status"     // GET
#define API_SLEEP_TIMEOUT "/api/sleep/timeout"    // POST {timeout_sec:uint}
#define API_SLEEP_RESET   "/api/sleep/reset"      // POST
#define API_SLEEP_SCHED   "/api/sleep/schedule"   // POST {after_sec:uint} or {wake_epoch:uint}

// ---------------- Power passthrough ----------------
#define API_POWER_INFO    "/api/power/info" // GET
#define API_POWER_CMD     "/api/power/cmd"  // POST {pwr_action:"..."}

// ---------------- Uploads & static assets ----------------
#define API_FILE_UPLOAD   "/upload"        // POST (chunked)
#define API_STATIC_ICONS  "/icons/"
#define API_STATIC_FONTS  "/fonts/"
#define API_FAVICON       "/favicon.ico"

// ---------------- Common JSON fields ----------------
#define J_OK        "ok"
#define J_ERR       "err"
#define J_ACTION    "action"
#define J_MAC       "mac"
#define J_TYPE      "type"
#define J_ID        "id"
#define J_STATE     "state"
#define J_MODE      "mode"
#define J_AUTO      "AUTO"
#define J_MANUAL    "MANUAL"
// Wi‑Fi creds
#define J_WIFI_SSID "ssid"
#define J_WIFI_PSK  "password"
#define J_AP_SSID   "ap_ssid"
#define J_AP_PSK    "ap_password"
// BLE
#define J_BLE_NAME  "ble_name"
#define J_BLE_PASS  "ble_password"
// ESP‑NOW
#define J_ESN_CH    "esn_ch"
// Topology / sequence
#define J_LINKS     "links"
#define J_PREV      "prev"
#define J_NEXT      "next"
#define J_START     "start"
#define J_DIR       "direction"
#define J_UP        "UP"
#define J_DOWN      "DOWN"
// Export / import
#define J_EXPORT    "export"
#define J_CONFIG    "config"
// Power
#define J_PWRACT    "pwr_action"
// Identity / versions
#define J_DEV_ID         "dev_id"
#define J_HOST_NAME      "host_name"
#define J_FRIENDLY_NAME  "friendly_name"
#define J_FW_VER         "fw_ver"
#define J_SW_VER         "sw_ver"
#define J_HW_VER         "hw_ver"
#define J_BUILD          "build"
#define J_PASS_PIN       "pass_pin"
// Web UI creds
#define J_WEB_USER       "web_user"
#define J_WEB_PASS       "web_pass"

#endif // WIFI_API_H

