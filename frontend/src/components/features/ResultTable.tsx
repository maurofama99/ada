import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from "@/components/ui/table"
import type { Result } from "@/types/Result"

export function ResultTable({ results, totRes }: { results: Result[], totRes: number }) {
    return (
        <div className="h-full flex flex-col">
            <h3 className="text-lg font-semibold p-4 pb-0 flex-shrink-0">Window Query Results</h3>
            <h4 className="text-sm font-medium px-4 flex-shrink-0 text-muted-foreground">{"Result count: " + results.length}</h4>
            <h4 className="text-sm font-medium px-4 pb-2 flex-shrink-0 text-muted-foreground">{"Running total: " + totRes}</h4>
            <div className="flex-1 overflow-auto">
                <Table>
                    <TableHeader>
                        <TableRow>
                            <TableHead>Source</TableHead>
                            <TableHead>Destination</TableHead>
                            <TableHead>Timestamp</TableHead>
                        </TableRow>
                    </TableHeader>
                    <TableBody>
                        {results.map((row) => (
                            <TableRow key={"res_" + row.s + "_" + row.d}>
                                <TableCell>{row.s}</TableCell>
                                <TableCell>{row.d}</TableCell>
                                <TableCell>{row.t}</TableCell>
                            </TableRow>
                        ))}
                    </TableBody>
                </Table>
            </div>
        </div>
    )
}