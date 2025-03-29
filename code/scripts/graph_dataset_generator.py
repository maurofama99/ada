import random

def generate_dynamic_graph(file_name, num_vertices, num_edges, num_labels):
    """
    Genera un grafo dinamico e salva gli archi in un file.

    Args:
        file_name (str): Nome del file di output.
        num_vertices (long long): Numero di vertici univoci.
        num_edges (long long): Numero di archi da generare.
    """
    edges = []
    timestamp = 0

    for _ in range(num_edges):
        # Genera due vertici casuali
        v1 = random.randint(1, num_vertices)
        v2 = random.randint(1, num_vertices)

        # Evita auto-anelli (opzionale, può essere rimosso se sono ammessi)
        while v1 == v2:
            v2 = random.randint(1, num_vertices)

        # Label fissa a 1 (può essere modificata per generare altre etichette)
        label = random.randint(1, num_labels)

        # Aggiorna il timestamp con un incremento casuale
        timestamp += random.randint(1, 1)

        # Aggiungi l'arco alla lista
        edges.append(f"{v1} {v2} {label} {timestamp}")

    # Salva gli archi in un file
    with open(file_name, "w") as file:
        file.write("\n".join(edges))

# Esempio di utilizzo
if __name__ == "__main__":
    output_file = "../dataset/debug_small.txt"
    unique_vertices = 10 # Numero di vertici univoci
    total_edges = 25     # Numero totale di archi
    total_labels = 2

    generate_dynamic_graph(output_file, unique_vertices, total_edges, total_labels)
    print(f"Grafo dinamico generato e salvato in {output_file}")
