export interface Edge {
    s: string
    d: string
    l: string
    t: number
    t_new?: number
}

export interface TEdge extends Edge {
    lives: number
    criteria: string
}

export interface SGEdge extends Edge {
    lives: number
    criteria: string
}

function isValidEdge(obj: any): obj is Edge {
    return obj &&
        's' in obj &&
        'd' in obj &&
        'l' in obj &&
        't' in obj &&
        typeof obj.t === 'number'
}

function isValidTEdge(obj: any): obj is TEdge {
    return isValidEdge(obj) &&
        'lives' in obj &&
        'criteria' in obj &&
        typeof obj.lives === 'number'
}

function isValidSGEdge(obj: any): obj is SGEdge {
    return isValidEdge(obj) &&
        'lives' in obj &&
        'criteria' in obj &&
        typeof obj.lives === 'number'
}

function normalizeEdge(raw: any): Edge | undefined {
    if (!isValidEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: Number(raw.t)
    }
}

function normalizeTEdge(raw: any): TEdge | undefined {
    if (!isValidTEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: Number(raw.t),
        lives: Number(raw.lives),
        criteria: String(raw.criteria)
    }
}

function normalizeSGEdge(raw: any): SGEdge | undefined {
    if (!isValidSGEdge(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        l: String(raw.l),
        t: Number(raw.t),
        lives: Number(raw.lives),
        criteria: String(raw.criteria)
    }
}

export { isValidEdge, isValidTEdge, isValidSGEdge, normalizeEdge, normalizeTEdge, normalizeSGEdge }