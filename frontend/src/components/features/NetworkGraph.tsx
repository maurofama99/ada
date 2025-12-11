import { forwardRef, useEffect, useImperativeHandle, useRef } from 'react';
import { DataSet, Network } from 'vis-network/standalone';
import { type Edge } from '@/types/Edge';

type NodeType = {
    id: string,
    label: string,
};

type EdgeType = {
    id: string,
    from: string,
    to: string,
    label?: string,
    arrows: string,
    color?: string,
};

interface NetworkHandle {
    addNode: (node: NodeType) => void;
    addEdge: (edge: Edge) => void;
    refresh: (edges: Edge[]) => void;
    clear: () => void;
};

const NetworkGraph = forwardRef<NetworkHandle>((props, ref) => {
    const container = useRef(null);
    const network = useRef<Network | null>(null);
    const edgesRef = useRef<DataSet<EdgeType> | null>(null);
    const nodesRef = useRef<DataSet<NodeType> | null>(null);

    function getNodeEdgeIds(edges: Edge[]) {
        const setNodes = new Set<string>()
        const setEdges = new Set<string>()
        edges.forEach(edge => {
            setNodes.add("net_" + edge.s)
            setNodes.add("net_" + edge.d)
            setEdges.add("net_" + edge.s + "_" + edge.d + "_" + edge.l)
        })
        return {
            nodeIds: [...setNodes],
            edgeIds: [...setEdges]
        }
    }

    useImperativeHandle(ref, () => ({
        addNode(node: NodeType) {
            nodesRef.current?.update(node);
        },
        addEdge(edge: Edge) {
            const edgeId = "net_" + edge.s + "_" + edge.d + "_" + edge.l;
            edgesRef.current?.update({
                id: edgeId,
                from: "net_" + edge.s,
                to: "net_" + edge.d,
                label: edge.l,
                arrows: "to",
                color: edge.lives !== undefined && edge.lives > 1 ? 'slategray' : "red"
            });
        },
        refresh(edges: Edge[]) {
            const { nodeIds, edgeIds } = getNodeEdgeIds(edges)
            edgesRef.current?.getIds().forEach(id => {
                if (!edgeIds.includes(id.toString())) {
                    edgesRef.current?.remove(id);
                }
            })
            nodesRef.current?.getIds().forEach(id => {
                if (!nodeIds.includes(id.toString())) {
                    nodesRef.current?.remove(id);
                }
            })
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

    return <div ref={container} style={{ height: '100%' }} />;
});

export { NetworkGraph };
export type { NetworkHandle };
