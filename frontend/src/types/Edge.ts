export interface Edge {
    s: string
    d: string
    l: string
    t: string
    lives?: number
}

function isValidEdge(obj: any): obj is Edge {
    return obj &&
        's' in obj &&
        'd' in obj &&
        'l' in obj &&
        't' in obj &&
        (!('lives' in obj) || typeof obj.lives === 'number')
}

function normalizeEdge(raw: any): Edge | undefined {
    if (!isValidEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: String(raw.t),
        lives: raw.lives !== undefined ? Number(raw.lives) : undefined
    }
}

function getNodeEdgeIds(edges: Edge[]) {
    const setNodes = new Set<string>()
    const setEdges = new Set<string>()
    edges.forEach(edge => {
        setNodes.add("net_" + edge.s)
        setNodes.add("net_" + edge.d)
        setEdges.add("net_" + edge.s + "_" + edge.d + "_" + edge.l)
    })
    return {
        nodeIds: [...setNodes],
        edgeIds: [...setEdges]
    }
}

export { isValidEdge, normalizeEdge, getNodeEdgeIds }