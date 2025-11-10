import {
    Table,
    TableBody,
    TableCaption,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from "@/components/ui/table"
import type { Edge } from '@/types/Edge'
import type { Window } from '@/types/Window'

export function WindowTable({ window }: { window?: Window }) {
    return (
        <Table>
            <TableCaption>{window ? "Window [" + window.open + ":" + window.close + ")" : "No Active Window"}</TableCaption>
            <TableHeader>
                <TableRow>
                    <TableHead>Source</TableHead>
                    <TableHead>Destination</TableHead>
                    <TableHead>Label</TableHead>
                    <TableHead>Timestamp</TableHead>
                </TableRow>
            </TableHeader>
            <TableBody>
                {window ? window.t_edges.map((row) => (
                    <TableRow key={"active_" + row.s + "_" + row.d}>
                        <TableCell>{row.s}</TableCell>
                        <TableCell>{row.d}</TableCell>
                        <TableCell>{row.l}</TableCell>
                        <TableCell>{row.t}</TableCell>
                    </TableRow>
                )) : null}
            </TableBody>
        </Table>
    )
}