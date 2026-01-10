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
  const { inputEdges, window, queryPattern, tEdges, sgEdges, results, totRes, isLoading, error, loadData } = useData()
  useEffect(() => {
    if (error) {
      toast.error(error.message)
    }
  }, [error])

  useEffect(() => {
    networkRef.current?.refresh(sgEdges);
    console.log(sgEdges);
    sgEdges.forEach((edge, index) => {
      networkRef.current?.addNode({ id: "net_" + edge.s, label: edge.s });
      networkRef.current?.addNode({ id: "net_" + edge.d, label: edge.d });
      networkRef.current?.addEdge(edge);
    });
  }, [window])

  useEffect(() => { // Single edge added to window
    const lastEdge = sgEdges.at(-1);
    if (lastEdge) {
      networkRef.current?.addNode({ id: "net_" + lastEdge.s, label: lastEdge.s });
      networkRef.current?.addNode({ id: "net_" + lastEdge.d, label: lastEdge.d });
      networkRef.current?.addEdge(lastEdge);
    }
  }, [sgEdges])

  return (
    <>
      <Toaster position="bottom-left" />
      <div className="flex flex-col gap-4 p-4 md:h-screen">
        <div className="bg-muted/50 min-h-min rounded-xl p-4 flex items-center justify-between overflow-hidden">
          <div className="flex flex-col w-max">
            <div className="text-lg font-semibold">Approximate RPQ Demo</div>
            <div className="text-sm opacity-50">Query pattern: ({queryPattern?.pattern} : {queryPattern?.mapping})</div>
          </div>
          <div className="flex gap-2">
            <Button className="font-bold" onClick={loadData}>Step</Button>
            <Button className="font-bold opacity-20" onClick={() => { }}>Reset</Button>
          </div>
        </div>
        <div className="grid gap-4 md:grid-cols-4 md:grid-rows-2 md:h-full md:flex-1 md:min-h-0">
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden md:h-full">
            <EdgeTable edges={inputEdges} />
          </div>
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden md:h-full">
            <WindowTable window={window} edges={tEdges} />
          </div>
          <div className="bg-muted/50 aspect-video rounded-xl md:col-span-2 md:row-span-2 md:h-full md:w-full">
            <NetworkGraph ref={networkRef} />
          </div>
          <div className="bg-muted/50 h-[50vh] rounded-xl overflow-hidden md:col-span-2 md:h-full">
            <ResultTable results={results} totRes={totRes} />
          </div>
        </div>
      </div>
    </>
  )
}

export default App