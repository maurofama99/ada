import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from "@/components/ui/table"
import type { Edge } from '@/types/Edge'

export function EdgeTable({ edges }: { edges: Edge[] }) {
    return (
        <div className="h-full flex flex-col">
            <h3 className="text-lg font-semibold p-4 pb-0 flex-shrink-0">Input Stream</h3>
            <h4 className="text-sm font-medium px-4 flex-shrink-0 text-muted-foreground">{"Count: " + edges.length}</h4>
            <div className="flex-1 overflow-auto">
                <Table>
                    <TableHeader>
                        <TableRow>
                            <TableHead>Src</TableHead>
                            <TableHead>Label</TableHead>
                            <TableHead>Dst</TableHead>
                            <TableHead>Time</TableHead>
                        </TableRow>
                    </TableHeader>
                    <TableBody>
                        {edges.slice().reverse().map((row) => (
                            <TableRow key={"input_" + row.s + "_" + row.d + "_" + row.l + "_" + row.t}>
                                <TableCell><span className="bg-blue-100 text-blue-700 px-2 py-1 rounded-full">{row.s}</span></TableCell>
                                <TableCell>{row.l} ➤</TableCell>
                                <TableCell><span className="bg-purple-100 text-purple-700 px-2 py-1 rounded-full">{row.d}</span></TableCell>
                                <TableCell>{row.t + (row.t_new ? " -> " + row.t_new : "")}</TableCell>
                            </TableRow>
                        ))}
                    </TableBody>
                </Table>
            </div>
        </div>
    )
}