import { forwardRef, useEffect, useImperativeHandle, useRef } from 'react';
import { DataSet, Network } from 'vis-network/standalone';

type NodeType = {
    id: number;
    label: string;
};

type EdgeType = {
    id: number;
    from: number;
    to: number;
};

interface NetworkHandle {
    addNode: (node: NodeType) => void;
}

const NetworkGraph = forwardRef<NetworkHandle>((props, ref) => {
    const container = useRef(null);
    const network = useRef<Network | null>(null);
    const edgesRef = useRef<DataSet<EdgeType> | null>(null);
    const nodesRef = useRef<DataSet<NodeType> | null>(null);

    useImperativeHandle(ref, () => ({
        addNode(node: NodeType) {
            nodesRef.current?.add(node);
        },
    }));

    useEffect(() => {
        if (!container.current) return;
        nodesRef.current = new DataSet([
            { id: 1, label: 'Node 1' },
            { id: 2, label: 'Node 2' },
            { id: 3, label: 'Node 3' }
        ]);

        edgesRef.current = new DataSet([
            { id: 1, from: 1, to: 2 },
            { id: 2, from: 1, to: 3 }
        ]);

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
