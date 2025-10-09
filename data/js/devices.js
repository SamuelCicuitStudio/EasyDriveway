// devices.js — device catalog & colors
export const DEVICE_TYPES = [
  { key: "sensor",    name: "Sensor",    colorVar: "--color-sensor" },
  { key: "processor", name: "Processor", colorVar: "--color-processor" },
  { key: "actuator",  name: "Actuator",  colorVar: "--color-actuator" },
  { key: "power",     name: "Power",     colorVar: "--color-power" },
  { key: "network",   name: "Network",   colorVar: "--color-network" },
  { key: "storage",   name: "Storage",   colorVar: "--color-storage" },
  { key: "interface", name: "Interface", colorVar: "--color-interface" },
  { key: "display",   name: "Display",   colorVar: "--color-display" },
  { key: "gateway",   name: "Gateway",   colorVar: "--color-gateway" },
  { key: "custom",    name: "Custom",    colorVar: "--color-custom" }
];

export function getCSSColor(colorVar){
  return getComputedStyle(document.documentElement).getPropertyValue(colorVar).trim() || "#ccc";
}
