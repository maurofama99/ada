import networkx as nx
import matplotlib.pyplot as plt

def read_edges_from_file(filename):
    edges = []
    with open(filename, 'r') as file:
        for line in file:
            source, destination, label, timestamp = line.split()
            edges.append((int(source), int(destination), (label, timestamp)))
    return edges

def draw_graph(edges):
    G = nx.DiGraph()

    for source, destination, label in edges:
        G.add_edge(source, destination, label=label)

    pos = nx.spring_layout(G, seed=42)
    labels = {(u, v): f"{d[0]}, {d[1]}" for u, v, d in G.edges(data='label')}

    plt.figure(figsize=(10, 6))
    nx.draw(G, pos, with_labels=True, node_color='lightblue', edge_color='gray', node_size=2000, font_size=10, font_weight='bold')
    nx.draw_networkx_edge_labels(G, pos, edge_labels=labels, font_size=8, font_color='red')
    plt.show()

if __name__ == "__main__":
    filename = "../dataset/debug_small.txt"  # Cambia con il nome del file di input
    edges = read_edges_from_file(filename)
    draw_graph(edges)
