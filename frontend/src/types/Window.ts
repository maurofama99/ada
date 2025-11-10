import { isValidEdge, normalizeEdge, type Edge } from '@/types/Edge'

export interface Window {
    open: string
    close: string
    t_edges: Edge[]
}

function isValidWindow(obj: any): obj is Window {
    return obj &&
        typeof obj.open === 'number' &&
        typeof obj.close === 'number' &&
        Array.isArray(obj.t_edges) &&
        obj.t_edges.every(isValidEdge)
}

function normalizeWindow(raw: any): Window | undefined {
    if (raw && 'prop_t_edges' in raw) {
        raw = { ...raw, t_edges: raw.prop_t_edges }
    }
    if (!isValidWindow(raw)) return undefined

    return {
        open: String(raw.open),
        close: String(raw.close),
        t_edges: raw.t_edges.map(normalizeEdge).filter((edge): edge is Edge => edge !== undefined)
    }
}


export { isValidWindow, normalizeWindow }