#!/bin/bash

SCRIPT_PATH="Makefile"

# Vérifier que le fichier Makefile existe
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "Erreur: Le fichier Makefile n'existe pas dans le répertoire courant."
    exit 1
fi
# Exécuter la commande make test
make test