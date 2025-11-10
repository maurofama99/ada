import type { Edge } from '@/types/Edge'
import type { Window } from '@/types/Window'


export interface ApiResponse {
    new_edge: Edge
    active_window?: Window
    t_edges?: Edge[]
    sg_edges?: Edge[]
}