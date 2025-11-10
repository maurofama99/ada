
export interface Window {
    open: string
    close: string
}

function isValidWindow(obj: any): obj is Window {
    return obj &&
        'open' in obj &&
        'close' in obj
}

function normalizeWindow(raw: any): Window | undefined {
    if (!isValidWindow(raw)) return undefined

    return {
        open: String(raw.open),
        close: String(raw.close)
    }
}


export { isValidWindow, normalizeWindow }