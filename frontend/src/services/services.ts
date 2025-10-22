import type { Edge } from '@/types/Edge'


// const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8000'
const API_BASE_URL = 'http://localhost:18080'

export async function fetchEdge(): Promise<Edge> {
  // const response = await fetch(`${API_BASE_URL}/proceed`)
  // if (!response.ok) {
  //   throw new Error('Failed to fetch data')
  // }
  // return response.json()
  return { v1: "A", v2: "B", ts: "2025-10-22T20:00:00Z", label: "first" }
}
