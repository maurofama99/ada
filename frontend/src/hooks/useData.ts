import { useState } from 'react'
import { fetchState } from '@/services/services'
import type { Edge, SGEdge, TEdge } from '@/types/Edge'
import type { Window } from '@/types/Window'
import type { Result } from '@/types/Result'
import type { QueryPattern } from '@/types/QueryPattern'

export function useData() {
    const [inputEdges, setInputEdges] = useState<Edge[]>([])
    const [window, setWindow] = useState<Window>()
    const [prevEdges, setPrevEdges] = useState<Map<string, TEdge>>()
    const [queryPattern, setQueryPattern] = useState<QueryPattern>()
    const [pruneCriteria, setPruneCriteria] = useState<string>()
    const [prunedCount, setPrunedCount] = useState<number>(0)
    const [tEdges, setTEdges] = useState<TEdge[]>([])
    const [sgEdges, setSGEdges] = useState<SGEdge[]>([])
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

            let currentPrevMap = prevEdges;

            if (result.active_window) { //refresh full data on new window
                console.log("New active window received, refreshing data" + result.sg_edges?.length)
                setWindow(result.active_window!)

                // set t_new property if edge existed in previous window
                const oldMap = new Map();
                for (const old of tEdges || []) {
                    oldMap.set(`${old.s}|${old.d}|${old.l}`, old);
                }
                setPrevEdges(oldMap);
                currentPrevMap = oldMap;

                // for (const e of result.t_edges || []) {
                //     if (e.lives === 1) {
                //         const match = oldMap.get(`${e.s}|${e.d}|${e.l}`)
                //         if (match) {
                //             e.t_new = e.t;
                //             e.t = match?.t;
                //         }
                //     }
                // }
                // result.t_edges?.sort((a, b) => a.t - b.t)
                // setTEdges(result.t_edges || [])
            }
            if (result.sg_edges !== undefined && result.t_edges !== undefined) { //incremental update on edges
                console.log("Else")
                setSGEdges(result.sg_edges || [])

                const uniqueEdges = [...new Map<string, TEdge>(
                    result.t_edges.map(e => [`${e.s}|${e.d}|${e.l}`, e])
                ).values()]

                if (currentPrevMap) {
                    for (const e of uniqueEdges) {
                        if (e.lives === 1) {
                            const match = currentPrevMap.get(`${e.s}|${e.d}|${e.l}`)
                            if (match) {
                                e.t_new = e.t;
                                e.t = match?.t;
                            }
                        }
                    }
                }

                uniqueEdges.sort((a, b) => a.t - b.t)
                setTEdges(uniqueEdges)

                // if (result.t_edges !== undefined && result.t_edges.length > 0) {
                //     setTEdges(prevTEdges => [...prevTEdges, ...result.t_edges!])
                // }
                // if (result.sg_edges !== undefined && result.sg_edges.length > 0) {
                //     setSGEdges(prevSGEdges => [...prevSGEdges, ...result.sg_edges!])
                // }
            }
            if (result.query_pattern !== undefined) {
                setQueryPattern(result.query_pattern)
            }
            if (result.prune_criteria !== undefined) {
                setPruneCriteria(result.prune_criteria)
            }
            if (result.tot_res !== undefined) {
                setTotRes(result.tot_res)
            }
            if (result.results !== undefined) {
                setResults(result.results)
            }
            if (result.pruned_count !== undefined) {
                setPrunedCount(result.pruned_count + prunedCount)
            }
        } catch (err) {
            // console.error('Error loading data:', err)
            setError(err as Error)
        } finally {
            setIsLoading(false)
        }
    }

    return { inputEdges, window, queryPattern, tEdges, sgEdges, results, totRes, isLoading, error, loadData, prunedCount, pruneCriteria }
}