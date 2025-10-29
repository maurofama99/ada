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
            if (result.new_edge) {
                setEdges(prevData => [...prevData, result.new_edge!])
            }
            if (result.active_window) {
                setWindow(result.active_window!)
            }
        } catch (err) {
            setError(err as Error)
        } finally {
            setIsLoading(false)
        }
    }

    return { edges, window, isLoading, error, loadData }
}