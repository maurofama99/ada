import { useState } from 'react'
import { fetchState } from '@/services/services'
import type { Edge } from '@/types/Edge'
import type { Window } from '@/types/Window'
import type { Result } from '@/types/Result'

export function useData() {
    const [inputEdges, setInputEdges] = useState<Edge[]>([])
    const [window, setWindow] = useState<Window>()
    const [tEdges, setTEdges] = useState<Edge[]>([])
    const [sgEdges, setSGEdges] = useState<Edge[]>([])
    const [results, setResults] = useState<Result[]>([])
    const [totRes, setTotRes] = useState<number>(0)
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError] = useState<Error | null>(null)

    async function loadData() {
        console.log("button clicked")
        setIsLoading(true)
        setError(null)

        try {
            const result = await fetchState()
            setInputEdges(prevEdges => [...prevEdges, result.new_edge!])
            if (result.active_window) { //refresh full data on new window
                console.log("New active window received, refreshing data" + result.sg_edges?.length)
                setWindow(result.active_window!)
                setTEdges(result.t_edges || [])
                setSGEdges(result.sg_edges || [])
                console.log("buh length: " + sgEdges.length)
            } else { //incremental update on edges
                console.log("Else")
                if (result.t_edges !== undefined && result.t_edges.length > 0) {
                    setTEdges(prevTEdges => [...prevTEdges, ...result.t_edges!])
                }
                if (result.sg_edges !== undefined && result.sg_edges.length > 0) {
                    setSGEdges(prevSGEdges => [...prevSGEdges, ...result.sg_edges!])
                }
            }
            if (result.tot_res !== undefined) {
                setTotRes(result.tot_res)
            }
            if (result.results !== undefined) {
                setResults(result.results)
            }
        } catch (err) {
            // console.error('Error loading data:', err)
            setError(err as Error)
        } finally {
            setIsLoading(false)
        }
    }

    return { inputEdges, window, tEdges, sgEdges, results, totRes, isLoading, error, loadData }
}