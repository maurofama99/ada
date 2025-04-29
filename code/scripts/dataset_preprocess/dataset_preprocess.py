def parse_label_map(config_file):
    """
    Legge il file di configurazione e restituisce la mappa label -> intero.
    Ritorna anche un valore di default specificato nella mappa.
    """
    label_map = {}
    default_value = -1  # Valore predefinito se non specificato nella mappa

    with open(config_file, 'r') as file:
        for line in file:
            line = line.strip()
            if line:
                parts = line.split()
                if len(parts) == 2:
                    key, value = parts
                    if key == "default":
                        default_value = int(value)
                    else:
                        label_map[key] = int(value)

    return label_map, default_value

def transform_file(input_file, output_file, label_map, default_value, max_lines):
    """
    Trasforma il file di input seguendo le specifiche e salva il risultato nel file di output.
    Utilizza un valore di default se un'etichetta non è trovata nella mappa.
    Rispetta il limite di righe specificato da max_lines.
    """
    with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
        for i, line in enumerate(infile):
            if max_lines != -1 and i >= max_lines:
                break
            parts = line.strip().split()
            if len(parts) == 4:
                num1, num2, num3, label = parts
                # Converti l'etichetta in intero usando la mappa, o usa il valore di default
                label_int = label_map.get(label, default_value)
                # Scrivi la riga trasformata nel file di output
                outfile.write(f"{num1} {num2} {label_int} {num3}\n")

def main():
    # File di configurazione
    config_file = "/Users/maurofama/Documents/phd/frames4pgs/CbAW4DGSP/code/scripts/dataset_preprocess/config_higgs.txt"
    # File di input e output
    input_file = "/Users/maurofama/Documents/phd/frames4pgs/CbAW4DGSP/code/dataset/higgs-activity/higgs-activity_time.txt"
    output_file = "/Users/maurofama/Documents/phd/frames4pgs/CbAW4DGSP/code/dataset/higgs-activity/higgs-activity_time_postprocess.txt"

    # Limite sul numero di righe del file di output (-1 per nessun limite)
    max_lines = 100000000000000  # Cambia questo valore come necessario

    # Leggi la mappa delle etichette e il valore di default
    label_map, default_value = parse_label_map(config_file)

    # Trasforma il file di input e crea l'output
    transform_file(input_file, output_file, label_map, default_value, max_lines)
    print(f"File trasformato salvato in: {output_file}")

if __name__ == "__main__":
    main()
