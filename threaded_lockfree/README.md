# Script do-it
Le script do-it lance la commande `make test` qui permet de compiler et d'exécuter les benchmarks.

## Compilation
Pour compiler le projet, utilisez la commande suivante:

```bash
make
```

## Exécution des benchmarks

### Commandes Makefile

#### Compilation et exécution complète

Ces commande compile le projet et exécute la suite complète de benchmarks.

```bash
./do-it.sh
```
ou juste:

```bash
make test
```
Conseil: Il est recommandé de lancer le benchmark complet sur la machine Xeon phi pour ne pas avoir de soucis de ressources.

#### Benchmarks individuels
```bash
make bench-full          # Tous les benchmarks (scalability, cutoff, threads)
make bench-scalability   # Test de scalabilité uniquement
make bench-cutoff        # Test d'impact du cutoff uniquement
make bench-threads       # Test de scaling des threads uniquement
```

#### Aide
```bash
make help
```
Affiche toutes les commandes disponibles et les options du script Python.

### Script Python (utilisation avancée)

Le script `benchmark_complete.py` peut être appelé directement avec de nombreuses options :

#### Arguments généraux
```bash
python3 benchmark_complete.py --test [scalability|cutoff|threads|all]  # Type de test (default: all)
python3 benchmark_complete.py --runs 10                                 # Nombre de runs par test (default: 5)
python3 benchmark_complete.py --threads 128                             # Threads par défaut (default: 60)
python3 benchmark_complete.py --cutoff 10                               # Cutoff par défaut (default: 8)
python3 benchmark_complete.py --cities 12                               # Nombre de villes pour tests cutoff/threads
```

#### Skip des méthodes
```bash
python3 benchmark_complete.py --skip-direct              # Skip la méthode direct
python3 benchmark_complete.py --skip-mutex               # Skip la méthode mutex
python3 benchmark_complete.py --skip-mutex-factorial     # Skip la méthode mutex_factorial
python3 benchmark_complete.py --skip-worksteal           # Skip la méthode worksteal
```

#### Personnalisation des ranges
```bash
python3 benchmark_complete.py --cities-range 5-12                    # Range: 5 à 12 villes
python3 benchmark_complete.py --cities-range 5,8,10,12,15            # Valeurs spécifiques
python3 benchmark_complete.py --cutoff-values 5-10                   # Cutoff de 5 à 10
python3 benchmark_complete.py --thread-counts 1,10,50,100,200        # Threads spécifiques
```

#### Exemples combinés
```bash
# Test de scalabilité rapide (3 runs, sans direct/mutex)
python3 benchmark_complete.py --test scalability --runs 3 --skip-direct --skip-mutex

# Test threads avec 12 villes et threads spécifiques
python3 benchmark_complete.py --test threads --cities 12 --thread-counts 1,20,40,60,80,100

# Test cutoff avec valeurs personnalisées
python3 benchmark_complete.py --test cutoff --cities 14 --cutoff-values 4,6,8,10,12

# Test complet avec 128 threads par défaut
python3 benchmark_complete.py --threads 128 --cutoff 10 --runs 10
```

### Configuration par défaut

Lorsque vous exécutez `make test` ou `make bench-full`, les tests suivants sont effectués :

- **Scalabilité des villes**: Test sur 2 à 16 villes, avec 60 threads et un cutoff de 8
- **Cutoff**: Test sur 14 villes, avec 60 threads, et des valeurs de cutoff allant de 1 à 13
- **Threads**: Test sur 15 villes, avec un cutoff de 8, et un nombre de threads allant de 1 à 256 (pas de 10). Pour 1 thread, la méthode direct est inclue mais pas pour les autres nombres de threads.

## Résultats
Les résultats des benchmarks sont enregistrés dans le dossier `benchmark` sous forme de fichiers JSON `benchmark_complete.json`. A chaque nouvelle exécution, le fichier est mis à jour avec les nouveaux résultats.