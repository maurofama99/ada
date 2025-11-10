import { forwardRef, useEffect, useImperativeHandle, useRef } from 'react';
import { DataSet, Network } from 'vis-network/standalone';

type NodeType = {
    id: string;
    label: string;
};

type EdgeType = {
    id: string;
    from: string;
    to: string;
    label?: string;
};

interface NetworkHandle {
    addNode: (node: NodeType) => void;
    addEdge: (edge: EdgeType) => void;
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
        addEdge(edge: EdgeType) {
            if (!edgesRef.current?.get(edge.id)) {
                edgesRef.current?.add(edge);
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
