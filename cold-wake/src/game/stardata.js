// The local jump network, shared by the bridge hologram and the route puzzle.
export const STAR_NODES = [
  { id: 'A', name: 'AURELIA', x: 0.07, y: 0.56, kind: 'start' },
  { id: 'B', name: 'KESSLER DRIFT', x: 0.25, y: 0.24 },
  { id: 'C', name: 'TARSIS GATE', x: 0.27, y: 0.8 },
  { id: 'D', name: 'VEIL NEBULA', x: 0.46, y: 0.52 },
  { id: 'E', name: 'BRANNOCK', x: 0.5, y: 0.13 },
  { id: 'F', name: 'LOW EMBER', x: 0.55, y: 0.87 },
  { id: 'G', name: 'PULSAR XK-9', x: 0.7, y: 0.5, kind: 'hazard' },
  { id: 'H', name: 'HALCYON RELAY', x: 0.79, y: 0.2 },
  { id: 'I', name: 'MERIDIAN STATION', x: 0.93, y: 0.62, kind: 'goal' },
];

export const STAR_EDGES = [
  ['A', 'B', 3], ['A', 'C', 2], ['A', 'D', 5], ['B', 'E', 2], ['B', 'D', 3], ['C', 'D', 2], ['C', 'F', 3],
  ['D', 'G', 2], ['D', 'E', 4], ['D', 'F', 3], ['E', 'H', 3], ['F', 'I', 6], ['G', 'I', 2], ['G', 'H', 2],
  ['H', 'I', 3], ['F', 'G', 3],
];

export const JUMP_FUEL = 11;

export function edgeCost(a, b) {
  for (const [p, q, c] of STAR_EDGES) if ((p === a && q === b) || (p === b && q === a)) return c;
  return null;
}
