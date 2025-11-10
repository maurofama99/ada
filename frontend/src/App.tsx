import { useRef, useEffect } from "react"
import { Button } from "@/components/ui/button"
import { EdgeTable } from "@/components/features/EdgeTable"
import { useData } from "./hooks/useData"
import { WindowTable } from "@/components/features/WindowTable"
import { NetworkGraph, type NetworkHandle } from "@/components/features/NetworkGraph"
import { toast, Toaster } from "sonner"

function App() {
  const networkRef = useRef<NetworkHandle>(null);
  const { edges, window, isLoading, error, loadData } = useData()
  useEffect(() => {
    if (error) {
      toast.error(error.message)
    }
  }, [error])

  return (
    <>
      <Toaster position="bottom-left" />
      <div className="flex flex-1 flex-col gap-4 p-4">
        <div className="grid auto-rows-min gap-4 md:grid-cols-3">
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden">
            <EdgeTable edges={edges} />
          </div>
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden">
            <WindowTable window={window} />
          </div>
          <div className="bg-muted/50 aspect-video rounded-xl">
            <NetworkGraph />
          </div>
        </div>
        <Button onClick={loadData}>Proceed</Button>
        <div className="bg-muted/50 min-h-[100vh] flex-1 rounded-xl md:min-h-min" />

      </div>
    </>
  )
}

export default App