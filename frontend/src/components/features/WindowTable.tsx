import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from "@/components/ui/table"
import type { Edge } from '@/types/Edge'
import type { Window } from '@/types/Window'

export function WindowTable({ window, edges }: { window?: Window, edges?: Edge[] }) {
    return (
        <div className="h-full flex flex-col">
            <h3 className="text-lg font-semibold p-4 pb-0 flex-shrink-0">Active Window (Timed Edge LinkedList)</h3>
            <h4 className="text-sm font-medium px-4 pb-2 flex-shrink-0 text-muted-foreground">{window ? "Window [" + window.open + ":" + window.close + ")" : "No Active Window"}</h4>
            <div className="flex-1 overflow-auto">
                <Table>
                    <TableHeader>
                        <TableRow>
                            <TableHead>Source</TableHead>
                            <TableHead>Destination</TableHead>
                            <TableHead>Label</TableHead>
                            <TableHead>Timestamp</TableHead>
                        </TableRow>
                    </TableHeader>
                    <TableBody>
                        {edges ? edges.slice().reverse().map((row) => (
                            <TableRow key={"active_" + row.s + "_" + row.d + "_" + row.l + "_" + row.t} className={row.lives !== undefined && row.lives > 1 ? "" : "text-red-600"}>
                                <TableCell>{row.s}</TableCell>
                                <TableCell>{row.d}</TableCell>
                                <TableCell>{row.l}</TableCell>
                                <TableCell>{row.t}</TableCell>
                            </TableRow>
                        )) : null}
                    </TableBody>
                </Table>
            </div>
        </div>
    )
}