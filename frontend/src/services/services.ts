import type { ApiResponse } from '@/types/ApiResponse'
import { isValidEdge, normalizeEdge, type Edge } from '@/types/Edge'
import { isValidQueryPattern, normalizeQueryPattern } from '@/types/QueryPattern'
import { isValidResult, normalizeResult } from '@/types/Result'
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

  if (isValidEdge(raw.new_edge) === false) {
    throw new Error('Invalid stream edge')
  }

  if (raw.active_window !== undefined && !isValidWindow(raw.active_window)) {
    throw new Error('Invalid active window')
  }

  if (raw.query_pattern !== undefined && !isValidQueryPattern(raw.query_pattern)) {
    throw new Error('Invalid query pattern')
  }

  if (raw.t_edges !== undefined && !Array.isArray(raw.t_edges) && !raw.t_edges.every(isValidEdge)) {
    throw new Error('Invalid t_edges')
  }

  if (raw.sg_edges !== undefined && !Array.isArray(raw.sg_edges) && !raw.sg_edges.every(isValidEdge)) {
    throw new Error('Invalid sg_edges')
  }

  if (raw.results !== undefined && !Array.isArray(raw.results) && !raw.results.every(isValidResult)) {
    throw new Error('Invalid results')
  }

  if (raw.tot_res !== undefined && typeof raw.tot_res !== 'number') {
    throw new Error('Invalid result count')
  }

  const cleanedResponse: ApiResponse = {
    new_edge: normalizeEdge(raw.new_edge)!,
    active_window: normalizeWindow(raw.active_window),
    query_pattern: normalizeQueryPattern(raw.query_pattern),
    t_edges: raw.t_edges !== undefined ? raw.t_edges.map(normalizeEdge) : undefined,
    sg_edges: raw.sg_edges !== undefined ? raw.sg_edges.map(normalizeEdge) : undefined,
    results: raw.results !== undefined ? raw.results.map(normalizeResult) : undefined,
    tot_res: raw.tot_res
  }

  console.log('Cleaned data:', cleanedResponse)
  return cleanedResponse
}
