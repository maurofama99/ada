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
            <TableCaption>Your data table</TableCaption>
            <TableHeader>
                <TableRow>
                    <TableHead>a</TableHead>
                    <TableHead>b</TableHead>
                    <TableHead>ts</TableHead>
                    <TableHead>label</TableHead>
                </TableRow>
            </TableHeader>
            <TableBody>
                {edges.map((row) => (
                    <TableRow key={row.v1 + "_" + row.v2}>
                        <TableCell>{row.v1}</TableCell>
                        <TableCell>{row.v2}</TableCell>
                        <TableCell>{row.ts}</TableCell>
                        <TableCell>{row.label}</TableCell>
                    </TableRow>
                ))}
            </TableBody>
        </Table>
    )
}