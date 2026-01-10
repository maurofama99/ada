import type { Edge, TEdge, SGEdge } from '@/types/Edge'
import type { Window } from '@/types/Window'
import type { Result } from './Result'
import type { QueryPattern } from './QueryPattern'

export interface ApiResponse {
    new_edge: Edge
    active_window?: Window
    query_pattern?: QueryPattern
    t_edges?: TEdge[]
    sg_edges?: SGEdge[]
    results?: Result[]
    tot_res?: number
}