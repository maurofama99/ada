import { forwardRef, useEffect, useImperativeHandle, useRef } from 'react';
import { DataSet, Network } from 'vis-network/standalone';
import type { Edge } from '@/types/Edge';

type NodeType = {
    id: string;
    label: string;
};

type EdgeType = {
    id: string;
    from: string;
    to: string;
    label?: string;
    arrows: string;
    color?: string;
};

interface NetworkHandle {
    addNode: (node: NodeType) => void;
    addEdge: (edge: Edge) => void;
    clear: () => void;
}

const NetworkGraph = forwardRef<NetworkHandle>((props, ref) => {
    const container = useRef(null);
    const network = useRef<Network | null>(null);
    const edgesRef = useRef<DataSet<EdgeType> | null>(null);
    const nodesRef = useRef<DataSet<NodeType> | null>(null);

    useImperativeHandle(ref, () => ({
        addNode(node: NodeType) {
            if (!nodesRef.current?.get(node.id)) {
                nodesRef.current?.add(node);
            }
        },
        addEdge(edge: Edge) {
            const edgeId = "net_" + edge.s + "_" + edge.d;
            if (!edgesRef.current?.get(edgeId)) {
                edgesRef.current?.add({
                    id: edgeId,
                    from: "net_" + edge.s,
                    to: "net_" + edge.d,
                    label: edge.l,
                    arrows: "to",
                    color: edge.lives !== undefined && edge.lives > 0 ? "" : "red"
                });
            }
        },
        clear() {
            nodesRef.current?.clear();
            edgesRef.current?.clear();
        },
    }));

    useEffect(() => {
        if (!container.current) return;
        nodesRef.current = new DataSet([]);
        edgesRef.current = new DataSet([]);

        const data = { nodes: nodesRef.current, edges: edgesRef.current };
        const options = {};

        network.current = new Network(container.current, data, options);

        return () => {
            if (network.current) {
                network.current.destroy();
            }
        };
    }, []);

    return <div ref={container} style={{ height: '400px' }} />;
});

export { NetworkGraph };
export type { NetworkHandle };
