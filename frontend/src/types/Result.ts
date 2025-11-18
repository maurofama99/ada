export interface Result {
    s: string
    d: string
    t: string
}

function isValidResult(obj: any): obj is Result {
    return obj &&
        's' in obj &&
        'd' in obj &&
        't' in obj
}

function normalizeResult(raw: any): Result | undefined {
    if (!isValidResult(raw)) return undefined

    return {
        s: String(raw.s),
        d: String(raw.d),
        t: String(raw.t)
    }
}

export { isValidResult, normalizeResult }