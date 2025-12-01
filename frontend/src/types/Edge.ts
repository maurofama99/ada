export interface Edge {
    s: string
    d: string
    l: string
    t: number
    lives?: number
    t_new?: number
}

function isValidEdge(obj: any): obj is Edge {
    return obj &&
        's' in obj &&
        'd' in obj &&
        'l' in obj &&
        't' in obj &&
        typeof obj.t === 'number' &&
        (!('lives' in obj) || typeof obj.lives === 'number')
}

function normalizeEdge(raw: any): Edge | undefined {
    if (!isValidEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: Number(raw.t),
        lives: raw.lives !== undefined ? Number(raw.lives) : undefined
    }
}

export { isValidEdge, normalizeEdge }