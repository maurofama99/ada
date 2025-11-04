export interface Edge {
    s: string
    d: string
    l: string
    t: string
}

function isValidEdge(obj: any): obj is Edge {
    return obj &&
        typeof obj.s === 'string' &&
        typeof obj.d === 'string' &&
        typeof obj.l === 'string' &&
        typeof obj.t === 'string'
}

export { isValidEdge }