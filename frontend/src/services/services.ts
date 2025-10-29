import type { ApiResponse } from '@/types/ApiResponse'
import { isValidEdge, type Edge } from '@/types/Edge'
import { isValidWindow, type Window } from '@/types/Window'


// const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8000'
const API_BASE_URL = 'http://localhost:18080'

export async function fetchState(): Promise<ApiResponse> {
  const response = await fetch(`${API_BASE_URL}/proceed`)
  console.log(response.ok)
  if (!response.ok) {
    throw new Error('Failed to fetch data')
  }
  const raw = await response.json()
  console.log('Raw fetched data:', raw)
  const cleanedResponse: ApiResponse = {
    new_edge: isValidEdge(raw.new_edge) ? raw.new_edge : undefined,
    active_window: isValidWindow(raw.active_window) ? raw.active_window : undefined,
  }
  console.log('Fetched data:', cleanedResponse)
  return cleanedResponse
}
