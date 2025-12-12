export interface QueryPattern {
    pattern: string
    mapping: string
}

function isValidQueryPattern(obj: any): obj is QueryPattern {
    return obj &&
        'pattern' in obj &&
        'mapping' in obj
}

function normalizeQueryPattern(raw: any): QueryPattern | undefined {
    if (!isValidQueryPattern(raw)) return undefined

    return {
        pattern: String(raw.pattern),
        mapping: String(raw.mapping)
    }
}


export { isValidQueryPattern, normalizeQueryPattern }