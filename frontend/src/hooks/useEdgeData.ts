import { useState } from 'react'
import { fetchEdge } from '@/services/services'
import type { Edge } from '@/types/Edge'

export function useEdgeData() {
    const [data, setData] = useState<Edge[]>([])
    const [isLoading, setIsLoading] = useState(true)
    const [error, setError] = useState<Error | null>(null)

    //   useEffect(() => {
    //     fetchEdge()
    //       .then(setData)
    //       .catch(setError)
    //       .finally(() => setIsLoading(false))
    //   }, [])

    async function loadData() {
        setIsLoading(true)
        setError(null)

        try {
            const result = await fetchEdge()
            setData(prevData => [...prevData, result])
        } catch (err) {
            setError(err as Error)
        } finally {
            setIsLoading(false)
        }
    }

    return { data, isLoading, error, loadData }
}