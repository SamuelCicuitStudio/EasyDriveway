// chain.js — derive a linear chain from edges (if possible)
export function computeLinearChain(nodes, edges){
  // In-degree for each node
  const indeg = new Map(nodes.map(n => [n.id, 0]));
  edges.forEach(e => indeg.set(e.to, (indeg.get(e.to) || 0) + 1));

  // Start nodes == indegree 0
  const starts = nodes.filter(n => (indeg.get(n.id) || 0) === 0);

  // pick first start (if many)
  if (starts.length === 0) return [];
  let current = starts[0];
  const chain = [current];

  // Follow single outgoing edges until none
  while (true){
    const nextEdge = edges.find(e => e.from === current.id);
    if (!nextEdge) break;
    const nextNode = nodes.find(n => n.id === nextEdge.to);
    if (!nextNode) break;
    chain.push(nextNode);
    current = nextNode;
  }
  return chain;
}
