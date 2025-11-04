import { isValidEdge, type Edge } from '@/types/Edge'

export interface Window {
    open: string
    close: string
    edges: Edge[]
}

function isValidWindow(obj: any): obj is Window {
    return obj &&
        typeof obj.open === 'string' &&
        typeof obj.close === 'string' &&
        Array.isArray(obj.edges) &&
        obj.edges.every(isValidEdge)
}

export { isValidWindow }