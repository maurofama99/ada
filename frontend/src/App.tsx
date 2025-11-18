import { useRef, useEffect } from "react"
import { Button } from "@/components/ui/button"
import { EdgeTable } from "@/components/features/EdgeTable"
import { ResultTable } from "@/components/features/ResultTable"
import { useData } from "./hooks/useData"
import { WindowTable } from "@/components/features/WindowTable"
import { NetworkGraph, type NetworkHandle } from "@/components/features/NetworkGraph"
import { toast, Toaster } from "sonner"

function App() {
  const networkRef = useRef<NetworkHandle>(null);
  const { inputEdges, window, tEdges, sgEdges, results, totRes, isLoading, error, loadData } = useData()
  useEffect(() => {
    if (error) {
      toast.error(error.message)
    }
  }, [error])

  useEffect(() => {
    networkRef.current?.clear();
    sgEdges.forEach((edge, index) => {
      networkRef.current?.addNode({ id: "net_" + edge.s, label: edge.s });
      networkRef.current?.addNode({ id: "net_" + edge.d, label: edge.d });
      networkRef.current?.addEdge({
        id: "net_" + edge.s + "_" + edge.d,
        from: "net_" + edge.s,
        to: "net_" + edge.d,
        label: edge.l,
        arrows: "to"
      });
    });
  }, [window])

  useEffect(() => {
    const lastEdge = sgEdges.at(-1);
    if (lastEdge) {
      networkRef.current?.addNode({ id: "net_" + lastEdge.s, label: lastEdge.s });
      networkRef.current?.addNode({ id: "net_" + lastEdge.d, label: lastEdge.d });
      networkRef.current?.addEdge({
        id: "net_" + lastEdge.s + "_" + lastEdge.d,
        from: "net_" + lastEdge.s,
        to: "net_" + lastEdge.d,
        label: lastEdge.l,
        arrows: "to"
      });
    }
  }, [sgEdges])

  return (
    <>
      <Toaster position="bottom-left" />
      <div className="flex flex-1 flex-col gap-4 p-4">
        <div className="grid auto-rows-min gap-4 md:grid-cols-4">
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden">
            <EdgeTable edges={inputEdges} />
          </div>
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden">
            <WindowTable window={window} edges={tEdges} />
          </div>
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden">
            <ResultTable results={results} totRes={totRes} />
          </div>
          <div className="bg-muted/50 aspect-video rounded-xl">
            <NetworkGraph ref={networkRef} />
          </div>
        </div>
        <Button onClick={loadData}>Proceed</Button>
        <div className="bg-muted/50 min-h-[100vh] flex-1 rounded-xl md:min-h-min" />

      </div>
    </>
  )
}

export default App