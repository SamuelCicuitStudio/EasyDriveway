// chain/svg.js — helpers to draw link paths
export function pathBetween(ax, ay, bx, by) {
  const dx = bx - ax;
  const dy = by - ay;
  const curvature = 0.5;
  const cx1 = ax + dx * curvature;
  const cy1 = ay;
  const cx2 = bx - dx * curvature;
  const cy2 = by;
  return `M ${ax} ${ay} C ${cx1} ${cy1}, ${cx2} ${cy2}, ${bx} ${by}`;
}
