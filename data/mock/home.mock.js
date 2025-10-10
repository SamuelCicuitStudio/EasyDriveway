// Lightweight mock so the Home page can render without a backend.
// If your real endpoints respond, this mock is not used.
window.HOME_MOCK = window.HOME_MOCK || {
  powerInfo: async () => ({
    vbus_mV: 12200, // 12.2 V
    ibus_mA: 480, // 0.48 A
    tempC: 34.5,
    on: true,
  }),
  peersList: async () => ({
    peers: [
      { type: "power" },
      { type: "sensor" },
      { type: "relay" },
      { type: "semux" },
      { type: "remux" },
    ],
  }),
  topologyGet: async () => ({
    links: [
      { from: "icm", to: "pms" },
      { from: "pms", to: "relay1" },
    ],
  }),
};

window.HOME_MOCK = window.HOME_MOCK || {};

window.HOME_MOCK.identity = async () => ({
  // SYS identity (NVSConfig.h)
  kind: "ICM",
  device_id: "NODE-0000",
  def_name: "ICM",
  hwrev: "1",
  swver: "0.0.0",
  build: "0",
});

window.HOME_MOCK.common = async () => ({
  // Config_Common.h
  config_partition: "config",
  log_dir: "/logs",
  log_file_path_icm: "/logs/icm.json",
  log_file_prefix_icm: "/logs/icm_",
});

window.HOME_MOCK.net = async () => ({
  // NVS NET defaults
  channel: 1,
});

window.HOME_MOCK.policy = async () => ({
  // IND policy defaults
  led_disabled: false,
  buzzer_disabled: false,
  rgb_active_low: true,
  rgb_feedback: true,
  buzzer_active_high: true,
  buzzer_feedback: true,
});
function postLogout() {
  window.location.href = "/data/login.html";
}
for (const id of ["logoutBtn", "logoutBtnLegacy"]) {
  const btn = document.getElementById(id);
  if (btn) btn.addEventListener("click", postLogout);
}
