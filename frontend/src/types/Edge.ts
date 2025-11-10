export interface Edge {
    s: string
    d: string
    l: string
    t: string
}

function isValidEdge(obj: any): obj is Edge {
    return obj &&
        's' in obj &&
        'd' in obj &&
        'l' in obj &&
        't' in obj
}

function normalizeEdge(raw: any): Edge | undefined {
    if (!isValidEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: String(raw.t)
    }
}

export { isValidEdge, normalizeEdge }