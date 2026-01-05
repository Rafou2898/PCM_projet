#!/usr/bin/env python3
"""
TSP Complete Benchmark Suite
Tests:
1. Impact du nombre de villes (scalabilité)
2. Impact du cutoff
3. Impact du nombre de threads
4. Calcul du speedup et de l'efficacité
"""

import subprocess
import json
import statistics
from pathlib import Path
import sys
import argparse

class CompleteBenchmark:
    def __init__(self, executable="./tsp", num_runs=5, default_threads=60, default_cutoff=8):
        self.executable = executable
        self.num_runs = num_runs
        self.default_threads = default_threads
        self.default_cutoff = default_cutoff
        self.all_results = {}
    
    def run_test(self, tsp_file, **kwargs):
        """Exécute un test avec les paramètres donnés"""
        cmd = [self.executable, tsp_file]
        
        # Ajouter les options
        if 'cities' in kwargs:
            cmd.extend(['--cities', str(kwargs['cities'])])
        if 'threads' in kwargs:
            cmd.extend(['--threads', str(kwargs['threads'])])
        if 'cutoff' in kwargs:
            cmd.extend(['--cutoff', str(kwargs['cutoff'])])
        if 'skip_direct' in kwargs:
            print("  Skipping direct method")
            cmd.extend(['--skip-direct'])
        if 'skip_mutex' in kwargs:
            print("  Skipping mutex method")
            cmd.extend(['--skip-mutex'])
        if 'skip_worksteal' in kwargs:
            print("  Skipping worksteal method")
            cmd.extend(['--skip-worksteal'])
        if 'skip_mutex_factorial' in kwargs:
            print("  Skipping mutex factorial method")
            cmd.extend(['--skip-mutex_fact'])
        if kwargs.get('no_cutoff', False):
            cmd.append('--no-cutoff')
        
        print(f"\n Commande: {' '.join(cmd)}")
        results = []
        for run in range(self.num_runs):
            try:
                print(f"run n°{run+1}.")
                result = subprocess.run(cmd, capture_output=True, text=True, 
                                        check=True)
                print(f"Sortie run {run+1}:\n{result.stdout.strip()}")
                parsed = self.parse_output(result.stdout)
                results.append(parsed)
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as e:
                print(f" Error in run {run+1}: {e}")
                return None
        
        return self.compute_stats(results)
    
    def parse_output(self, output):
        """Parse la sortie du programme"""
        results = {}
        for line in output.strip().split('\n'):
            if 't:' in line:
                # Extraire le nom de la méthode (avant le premier ':')
                method = line.split(':')[0].strip()
                
                # Extraire le temps (après 't:')
                time_part = line.split('t:')[-1].strip()
                
                try:
                    results[method] = float(time_part)
                except ValueError:
                    print(f"Couldn't parse time from line: {line}")
                    continue
        
        return results
    
    def compute_stats(self, results):
        """Calcule les statistiques"""
        stats = {}
        methods = set()
        for r in results:
            methods.update(r.keys())
        
        for method in methods:
            times = [r[method] for r in results if method in r]
            if times:
                stats[method] = {
                    'mean': statistics.mean(times),
                    'median': statistics.median(times),
                    'stdev': statistics.stdev(times) if len(times) > 1 else 0,
                    'min': min(times),
                    'max': max(times)
                }
        
        return stats
    
    def test_scalability(self, cities_range=None, skip_methods=None):
        """Test: Impact du nombre de villes"""
        print("\n" + "="*80)
        print("TEST SCALABILITÉ")
        print("="*80)
        
        if cities_range is None:
            cities_range = [2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]
        
        results = {}
        skip_methods = skip_methods or {}
        
        for cities in cities_range:
            tsp_file = f"./dj{cities:02d}.tsp"
            if not Path(tsp_file).exists():
                print(f"  {tsp_file} not found, using ./dj38.tsp")
                tsp_file = f"./dj38.tsp"
            
            print(f"Testing {cities} cities...", end=" ", flush=True)
            stats = self.run_test(tsp_file, cities=cities, threads=self.default_threads, 
                                cutoff=self.default_cutoff, **skip_methods)
            
            if stats:
                results[cities] = stats
                print(f" (direct: {stats.get('direct', {}).get('mean', 0):.4f}s, "
                      f"worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['scalability'] = results
        self.print_scalability_results(results)
        return results
    
    def test_cutoff_impact(self, cities=None, cutoff_values=None, skip_methods=None):
        """Test: Impact du cutoff"""
        print("\n" + "="*80)
        print("TEST IMPACT DU CUTOFF")
        print("="*80)
        
        if cities is None:
            cities = 14
        
        if cutoff_values is None:
            cutoff_values = [1,2,3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
        
        results = {}
        skip_methods = skip_methods or {}
        
        tsp_file = f"./dj{cities:02d}.tsp"
        if not Path(tsp_file).exists():
            print(f"  {tsp_file} not found, using ./dj38.tsp")
            tsp_file = "./dj38.tsp"
        
        # Test avec différentes valeurs de cutoff
        for cutoff in cutoff_values:
            print(f"Testing cutoff={cutoff}...", end=" ", flush=True)
            stats = self.run_test(tsp_file, cities=cities, threads=self.default_threads, 
                                cutoff=cutoff, skip_direct=True, skip_mutex=True, skip_mutex_factorial=True, **skip_methods)
            
            if stats:
                results[f'cutoff_{cutoff}'] = stats
                print(f" (worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['cutoff'] = results
        self.print_cutoff_results(results, cities)
        return results
    
    def test_thread_scaling(self, cities=None, thread_counts=None, skip_methods=None):
        """Test: Impact du nombre de threads"""
        print("\n" + "="*80)
        print("TEST SCALING DES THREADS")
        print("="*80)
        
        if cities is None:
            cities = 15
        
        if thread_counts is None:
            thread_counts = [1] + list(range(10, 257, 10)) + [256]
        
        results = {}
        skip_methods = skip_methods or {}
        
        tsp_file = f"./dj{cities:02d}.tsp"
        if not Path(tsp_file).exists():
            print(f"  {tsp_file} not found, using ./dj38.tsp")
            tsp_file = "./dj38.tsp"
        
        first_direct = True
        
        for threads in thread_counts:
            print(f"Testing {threads} threads...", end=" ", flush=True)
            
            # Skip direct sauf pour le premier run (référence)
            current_skip = skip_methods.copy()
            if not first_direct or 'skip_direct' in skip_methods:
                current_skip['skip_direct'] = True
            first_direct = False
            
            stats = self.run_test(tsp_file, cities=cities, threads=threads, 
                                cutoff=self.default_cutoff, **current_skip)            
            if stats:
                results[threads] = stats
                print(f" (direct: {stats.get('direct', {}).get('mean', 0):.4f}s, "
                      f"mutex: {stats.get('mutex', {}).get('mean', 0):.4f}s, "
                      f"mutex_factorial: {stats.get('mutex_factorial', {}).get('mean', 0):.4f}s, "
                      f"worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['threads'] = results
        self.print_thread_results(results)
        return results
        
    def print_scalability_results(self, results):
        """Affiche les résultats de scalabilité"""
        print("\nRésultats Scalabilité:")
        print(f"{'Cities':<10} {'Direct (s)':<12} {'Mutex (s)':<12}  {'Mutex factorial (s)': <12} {'WorkSteal (s)':<12} {'Speedup WS':<12}")
        print("-" * 60)
        
        for cities in sorted(results.keys()):
            r = results[cities]
            direct = r.get('direct', {}).get('mean', 0)
            mutex = r.get('mutex', {}).get('mean', 0)
            mutex_factorial = r.get('mutex_factorial', {}).get('mean',0)
            ws = r.get('worksteal', {}).get('mean', 0)
            speedup = direct / ws if ws > 0 else 0
            
            print(f"{cities:<10} {direct:<12.6f} {mutex:<12.6f} {mutex_factorial:<12.6f} {ws:<12.6f} {speedup:<12.2f}x")
    
    def print_cutoff_results(self, results, cities):
        """Affiche les résultats du cutoff"""
        print("\nRésultats Impact Cutoff:")
        print(f"Cities: {cities}")
        print(f"{'Cutoff':<15} {'Direct (s)':<12} {'Mutex (s)':<12} {'Mutex_factorial (s)':<12} {'WorkSteal (s)':<12}")
        print("-" * 55)
        
        for key in sorted(results.keys()):
            r = results[key]
            direct = r.get('direct', {}).get('mean', 0)
            mutex = r.get('mutex', {}).get('mean', 0)
            mutex_factorial = r.get('mutex_factorial', {}).get('mean',0)
            ws = r.get('worksteal', {}).get('mean', 0)
            
            print(f"{key:<15} {direct:<12.6f} {mutex:<12.6f} {mutex_factorial:<12.6f} {ws:<12.6f}")
    
    def print_thread_results(self, results):
        """Affiche les résultats du scaling des threads"""
        print("\n Résultats Thread Scaling:")
        print(f"{'Threads':<10} {'Mutex (s)':<12} {'Mutex Factorial (s)':<12} {'WorkSteal (s)':<12} {'Speedup M':<12} {'Speedup WS':<12}")
        print("-" * 60)
        
        base_mutex = results.get(1, {}).get('mutex', {}).get('mean', 1)
        base_mutex_factorial = results.get(1, {}).get('mutex_factorial', {}).get('mean', 1)
        base_ws = results.get(1, {}).get('worksteal', {}).get('mean', 1)
        
        for threads in sorted(results.keys()):
            r = results[threads]
            mutex = r.get('mutex', {}).get('mean', 0)
            mutex_factorial = r.get('mutex_factorial', {}).get('mean', 0)
            ws = r.get('worksteal', {}).get('mean', 0)
            
            speedup_m = base_mutex / mutex if mutex > 0 else 0
            speedup_ws = base_ws / ws if ws > 0 else 0
            
            print(f"{threads:<10} {mutex:<12.6f} {mutex_factorial:<12.6f} {ws:<12.6f} {speedup_m:<12.2f}x {speedup_ws:<12.2f}x")
    
    def save_all_results(self):
        """Sauvegarde tous les résultats"""
        with open('benchmark/benchmark_complete.json', 'w') as f:
            json.dump(self.all_results, f, indent=2)
        print(f"\n Tous les résultats sauvegardés dans benchmark/benchmark_complete.json")
    
    def generate_report(self):
        """Génère un rapport texte complet"""
        with open('benchmark/benchmark_report.txt', 'w') as f:
            f.write("="*80 + "\n")
            f.write("TSP WORK-STEALING BENCHMARK REPORT\n")
            f.write("="*80 + "\n\n")
            
            if 'speedup' in self.all_results:
                f.write("SPEEDUP SUMMARY:\n")
                f.write("-"*40 + "\n")
                for cities, r in sorted(self.all_results['speedup'].items()):
                    f.write(f"{cities} cities: {r['speedup_ws']:.2f}x speedup, "
                           f"{r['efficiency_ws']:.2%} efficiency\n")
                f.write("\n")
            
            if 'scalability' in self.all_results:
                f.write("SCALABILITY:\n")
                f.write("-"*40 + "\n")
                for cities, r in sorted(self.all_results['scalability'].items()):
                    ws = r.get('worksteal', {}).get('mean', 0)
                    f.write(f"{cities} cities: {ws:.6f}s\n")
                f.write("\n")
def main():
    parser = argparse.ArgumentParser(description='TSP Complete Benchmark Suite')
    
    # Arguments généraux
    parser.add_argument('--test', '-t', choices=['scalability', 'cutoff', 'threads', 'all'], 
                       default='all', help='Type de test à exécuter (default: all)')
    parser.add_argument('--runs', '-r', type=int, default=5, 
                       help='Nombre de runs par test (default: 5)')
    parser.add_argument('--threads', type=int, default=60, 
                       help='Nombre de threads par défaut (default: 60)')
    parser.add_argument('--cutoff', '-c', type=int, default=8, 
                       help='Cutoff par défaut (default: 8)')
    parser.add_argument('--cities', type=int, help='Nombre de villes (pour tests cutoff/threads)')
    
    # Arguments pour skip
    parser.add_argument('--skip-direct', action='store_true', 
                       help='Skip la méthode direct')
    parser.add_argument('--skip-mutex', action='store_true', 
                       help='Skip la méthode mutex')
    parser.add_argument('--skip-mutex-factorial', action='store_true', 
                       help='Skip la méthode mutex_factorial')
    parser.add_argument('--skip-worksteal', action='store_true', 
                       help='Skip la méthode worksteal')
    
    # Arguments pour personnaliser les ranges
    parser.add_argument('--cities-range', type=str, 
                       help='Range de villes pour scalabilité (ex: 2-17 ou 2,5,10,15)')
    parser.add_argument('--cutoff-values', type=str, 
                       help='Valeurs de cutoff à tester (ex: 1-13 ou 1,5,10)')
    parser.add_argument('--thread-counts', type=str, 
                       help='Valeurs de threads à tester (ex: 1-256 ou 1,10,50,100)')
    
    args = parser.parse_args()
    
    print("TSP Complete Benchmark Suite")
    print("="*80)
    print(f"Configuration:")
    print(f"  Runs par test: {args.runs}")
    print(f"  Threads par défaut: {args.threads}")
    print(f"  Cutoff par défaut: {args.cutoff}")
    print("="*80)
    
    # Construire le dictionnaire des méthodes à skip
    skip_methods = {}
    if args.skip_direct:
        skip_methods['skip_direct'] = True
    if args.skip_mutex:
        skip_methods['skip_mutex'] = True
    if args.skip_mutex_factorial:
        skip_methods['skip_mutex_factorial'] = True
    if args.skip_worksteal:
        skip_methods['skip_worksteal'] = True
    
    # Créer le benchmark
    benchmark = CompleteBenchmark(
        executable="./tsp", 
        num_runs=args.runs,
        default_threads=args.threads,
        default_cutoff=args.cutoff
    )
    
    # Parser les ranges
    def parse_range(range_str):
        """Parse une range comme '2-17' ou '2,5,10,15'"""
        if '-' in range_str:
            start, end = map(int, range_str.split('-'))
            return list(range(start, end + 1))
        else:
            return [int(x) for x in range_str.split(',')]
    
    # Exécuter les tests demandés
    if args.test in ['scalability', 'all']:
        cities_range = parse_range(args.cities_range) if args.cities_range else None
        benchmark.test_scalability(cities_range=cities_range, skip_methods=skip_methods)
    
    if args.test in ['cutoff', 'all']:
        cutoff_values = parse_range(args.cutoff_values) if args.cutoff_values else None
        cities = args.cities if args.cities else 14
        benchmark.test_cutoff_impact(cities=cities, cutoff_values=cutoff_values, 
                                     skip_methods=skip_methods)
    
    if args.test in ['threads', 'all']:
        thread_counts = parse_range(args.thread_counts) if args.thread_counts else None
        cities = args.cities if args.cities else 15
        benchmark.test_thread_scaling(cities=cities, thread_counts=thread_counts, 
                                     skip_methods=skip_methods)
    
    # Sauvegarder les résultats
    benchmark.save_all_results()
    benchmark.generate_report()
    
    print("\n✓ Benchmarking terminé!")
    print("\nFichiers générés:")
    print("  - benchmark_complete.json (toutes les données)")
    print("  - benchmark_report.txt (rapport)")

if __name__ == "__main__":
    main()
