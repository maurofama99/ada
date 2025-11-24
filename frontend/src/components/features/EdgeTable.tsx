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
            <h3 className="text-lg font-semibold p-4 pb-2 flex-shrink-0">Input Stream</h3>
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
                        {edges.slice().reverse().map((row) => (
                            <TableRow key={"input_" + row.s + "_" + row.d + "_" + row.l + "_" + row.t}>
                                <TableCell>{row.s}</TableCell>
                                <TableCell>{row.d}</TableCell>
                                <TableCell>{row.l}</TableCell>
                                <TableCell>{row.t}</TableCell>
                            </TableRow>
                        ))}
                    </TableBody>
                </Table>
            </div>
        </div>
    )
}