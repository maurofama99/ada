import type { ApiResponse } from '@/types/ApiResponse'
import { isValidEdge, normalizeEdge, type Edge } from '@/types/Edge'
import { isValidWindow, normalizeWindow, type Window } from '@/types/Window'


// const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8000'
const API_BASE_URL = 'http://localhost:18080'

export async function fetchState(): Promise<ApiResponse> {
  const response = await fetch(`${API_BASE_URL}/proceed`)

  if (!response.ok) {
    throw new Error('Failed to fetch data: ' + response.status)
  }

  const raw = await response.json()
  console.log('Raw fetched data:', raw)

  if (isValidEdge(raw.new_edge) === false ||
    (raw.active_window !== undefined && normalizeWindow(raw.active_window) === undefined) ||
    (raw.t_edge !== undefined && isValidEdge(raw.t_edge) === false) ||
    (raw.sg_edge !== undefined && isValidEdge(raw.sg_edge) === false)
  ) {
    throw new Error('Invalid data received from API')
  }

  const cleanedResponse: ApiResponse = {
    new_edge: normalizeEdge(raw.new_edge)!,
    active_window: normalizeWindow(raw.active_window),
    t_edge: normalizeEdge(raw.t_edge),
    sg_edge: normalizeEdge(raw.sg_edge)
  }

  console.log('Cleaned data:', cleanedResponse)
  return cleanedResponse
}
