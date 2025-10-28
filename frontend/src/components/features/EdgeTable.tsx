import {
    Table,
    TableBody,
    TableCaption,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from "@/components/ui/table"
import { useEdgeData } from "@/hooks/useEdgeData"
import type { Edge } from '@/types/Edge'

export function EdgeTable({ edges }: { edges: Edge[] }) {
    return (
        <Table>
            <TableCaption>Input Stream</TableCaption>
            <TableHeader>
                <TableRow>
                    <TableHead>Source</TableHead>
                    <TableHead>Destination</TableHead>
                    <TableHead>Label</TableHead>
                    <TableHead>Timestamp</TableHead>
                </TableRow>
            </TableHeader>
            <TableBody>
                {edges.map((row) => (
                    <TableRow key={row.s + "_" + row.d}>
                        <TableCell>{row.s}</TableCell>
                        <TableCell>{row.d}</TableCell>
                        <TableCell>{row.l}</TableCell>
                        <TableCell>{row.t}</TableCell>
                    </TableRow>
                ))}
            </TableBody>
        </Table>
    )
}