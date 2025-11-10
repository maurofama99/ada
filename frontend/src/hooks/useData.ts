import { useState } from 'react'
import { fetchState } from '@/services/services'
import type { Edge } from '@/types/Edge'
import type { Window } from '@/types/Window'

export function useData() {
    const [edges, setEdges] = useState<Edge[]>([])
    const [window, setWindow] = useState<Window>()
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError] = useState<Error | null>(null)

    async function loadData() {
        console.log("button clicked")
        setIsLoading(true)
        setError(null)

        try {
            const result = await fetchState()
            setEdges(prevEdges => [...prevEdges, result.new_edge!])
            if (result.active_window) {
                setWindow(result.active_window!)
            } else if (window !== undefined) {
                if (result.t_edge !== undefined) {
                    setWindow(prevWindow => {
                        return {
                            ...prevWindow!,
                            t_edges: [...prevWindow!.t_edges, result.t_edge!]
                        }
                    })
                }
            }
        } catch (err) {
            // console.error('Error loading data:', err)
            setError(err as Error)
        } finally {
            setIsLoading(false)
        }
    }

    return { edges, window, isLoading, error, loadData }
}