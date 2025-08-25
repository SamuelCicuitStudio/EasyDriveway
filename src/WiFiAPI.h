/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : WiFiAPI.h
 *  Purpose     : Centralize HTTP endpoints & JSON field names
 **************************************************************/
#ifndef WIFI_API_H
#define WIFI_API_H

// ---- REST endpoints (all JSON unless noted) ----
#define API_ROOT                    "/"
#define API_SETTINGS_HTML           "/settings"               // text/html
#define API_WIFI_PAGE_HTML          "/wifiCredentialsPage"    // text/html
#define API_THANKYOU_HTML           "/thankyou"               // text/html

#define API_CFG_LOAD                "/api/config/load"        // GET
#define API_CFG_SAVE                "/api/config/save"        // POST { wifi/ap/ble/esnChannel... }
#define API_CFG_EXPORT              "/api/config/export"      // GET   -> JSON blob
#define API_CFG_IMPORT              "/api/config/import"      // POST  (body JSON)
#define API_CFG_FACTORY_RESET       "/api/config/factory_reset" // POST

#define API_PEERS_LIST              "/api/peers/list"         // GET
#define API_PEER_PAIR               "/api/peer/pair"          // POST {mac,type}
#define API_PEER_REMOVE             "/api/peer/remove"        // POST {mac}

#define API_MODE_SET                "/api/mode/set"           // POST {mode:"AUTO|MANUAL"}
#define API_RELAY_SET               "/api/relay/set"          // POST {mac|id, state:0|1} manual override
#define API_SENSOR_MODE             "/api/sensor/mode"        // POST {mac|id, mode:"AUTO|MANUAL"}
#define API_SENSOR_TRIGGER          "/api/sensor/trigger"     // POST {mac|id} (test trigger)

#define API_TOPOLOGY_SET            "/api/topology/set"       // POST {links: [...]}
#define API_TOPOLOGY_GET            "/api/topology/get"       // GET

#define API_SEQUENCE_START          "/api/sequence/start"     // POST {start:"ENTRANCE|PARKING|mac|id", direction:"UP|DOWN"}
#define API_SEQUENCE_STOP           "/api/sequence/stop"      // POST

#define API_POWER_INFO              "/api/power/info"         // GET
#define API_POWER_CMD               "/api/power/cmd"          // POST {action:"SHUTDOWN|REBOOT|PING"...}

#define API_FILE_UPLOAD             "/upload"                 // POST (chunked)
#define API_STATIC_ICONS            "/icons/"
#define API_STATIC_FONTS            "/fonts/"
#define API_FAVICON                 "/favicon.ico"

// ---- JSON fields / enums ----
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

#define J_WIFI_SSID "ssid"
#define J_WIFI_PSK  "password"
#define J_AP_SSID   "ap_ssid"
#define J_AP_PSK    "ap_password"
#define J_BLE_NAME  "ble_name"
#define J_BLE_PASS  "ble_password"
#define J_ESN_CH    "esn_ch"

#define J_LINKS     "links"
#define J_PREV      "prev"
#define J_NEXT      "next"
#define J_START     "start"
#define J_DIR       "direction"
#define J_UP        "UP"
#define J_DOWN      "DOWN"

#define J_EXPORT    "export"
#define J_CONFIG    "config"

// power
#define J_PWRACT    "pwr_action"

#endif // WIFI_API_H
