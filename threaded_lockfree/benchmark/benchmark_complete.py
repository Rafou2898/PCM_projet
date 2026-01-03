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

class CompleteBenchmark:
    def __init__(self, executable="../tsp", num_runs=5):
        self.executable = executable
        self.num_runs = num_runs
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
        if kwargs.get('no_cutoff', False):
            cmd.append('--no-cutoff')
        
        cmd.append('--quiet')
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
            # Format attendu: "direct: {2101: 0, 1, 5, 7, 6, 4, 2, 3, 0} t:0.000928738"
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
    
    def test_scalability(self):
        """Test 1: Impact du nombre de villes"""
        print("\n" + "="*80)
        print(" TEST 1: SCALABILITÉ (Impact du nombre de villes)")
        print("="*80 + "\n")
        
        results = {}
        cities_range = [15]
        
        for cities in cities_range:
            tsp_file = f"../dj{cities:02d}.tsp"
            if not Path(tsp_file).exists():
                print(f"  {tsp_file} not found, using ../dj38.tsp")
                tsp_file = f"../dj38.tsp"
                
                
            
            print(f"Testing {cities} cities...", end=" ", flush=True)
            stats = self.run_test(tsp_file, cities=cities, threads=150, cutoff=8, skip_mutex=True, skip_direct=True, skip_worksteal=True)
            
            if stats:
                results[cities] = stats
                print(f" (direct: {stats.get('direct', {}).get('mean', 0):.4f}s, "
                      f"worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['scalability'] = results
        self.print_scalability_results(results)
        return results
    
    def test_cutoff_impact(self):
        """Test 2: Impact du cutoff"""
        print("\n" + "="*80)
        print("  TEST 2: IMPACT DU CUTOFF")
        print("="*80 + "\n")
        
        results = {}
        cutoff_values = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        tsp_file = "../dj14.tsp"
        cities = tsp_file.split('dj')[-1].split('.tsp')[0]
        if not Path(tsp_file).exists():
            tsp_file = "../dj38.tsp"
            
            

        
        # Test avec différentes valeurs de cutoff
        for cutoff in cutoff_values:
            print(f"Testing cutoff={cutoff}...", end=" ", flush=True)
            stats = self.run_test(tsp_file, threads=256, cutoff=cutoff, skip_direct=True, skip_mutex=True)
            
            if stats:
                results[f'cutoff_{cutoff}'] = stats
                print(f" (worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['cutoff'] = results
        cities = tsp_file.split('dj')[-1].split('.tsp')[0]
        self.print_cutoff_results(results, cities)
        return results
    
    def test_thread_scaling(self):
        """Test 3: Impact du nombre de threads"""
        print("\n" + "="*80)
        print(" TEST 3: SCALING DES THREADS")
        print("="*80 + "\n")
        
        results = {}
        #increment from 1 to 256 by 10
        thread_counts = list(range(0, 257, 10))
        #thread_counts = [2,4,6,8,10,12,14]
        #thread_counts = [256]
        tsp_file = "../dj15.tsp"
        
        cities = tsp_file.split('dj')[-1].split('.tsp')[0]
        if not Path(tsp_file).exists():
            tsp_file = "../dj38.tsp"
            
            
        
        for threads in thread_counts:
            print(f"Testing {threads} threads...", end=" ", flush=True)
            stats = self.run_test(tsp_file, cities=cities, threads=threads, skip_direct=True, cutoff=8)
            
            if stats:
                results[threads] = stats
                print(f" (direct: {stats.get('direct', {}).get('mean', 0):.4f}s, "
                      f"mutex: {stats.get('mutex', {}).get('mean', 0):.4f}s, "
                      f"worksteal: {stats.get('worksteal', {}).get('mean', 0):.4f}s)")
            else:
                print("aucune stats")
        
        self.all_results['threads'] = results
        self.print_thread_results(results)
        return results
    
    def test_speedup(self):
        """Test 4: Calcul du speedup"""
        print("\n" + "="*80)
        print("⚡ TEST 4: SPEEDUP ET EFFICACITÉ")
        print("="*80 + "\n")
        
        results = {}
        cities_range = [5, 8, 10, 12]
        
        for cities in cities_range:
            tsp_file = f"../dj{cities:02d}.tsp"
            if not Path(tsp_file).exists():
                continue
            num_threads  = 256
            print(f"Testing {cities} cities...", end=" ", flush=True)
            stats = self.run_test(tsp_file, threads=num_threads, cutoff=5)
            
            if stats and 'direct' in stats and 'worksteal' in stats:
                t_direct = stats['direct']['mean']
                t_mutex = stats.get('mutex', {}).get('mean', 0)
                t_ws = stats['worksteal']['mean']
                
                speedup_mutex = t_direct / t_mutex if t_mutex > 0 else 0
                speedup_ws = t_direct / t_ws
                efficiency_ws = speedup_ws / num_threads  # 256 threads
                
                results[cities] = {
                    'direct': t_direct,
                    'mutex': t_mutex,
                    'worksteal': t_ws,
                    'speedup_mutex': speedup_mutex,
                    'speedup_ws': speedup_ws,
                    'efficiency_ws': efficiency_ws
                }
                
                print(f"(speedup: {speedup_ws:.2f}x, efficiency: {efficiency_ws:.2f})")
            else:
                print("aucune stats")
        
        self.all_results['speedup'] = results
        self.print_speedup_results(results)
        return results
    
    def print_scalability_results(self, results):
        """Affiche les résultats de scalabilité"""
        print("\nRésultats Scalabilité:")
        print(f"{'Cities':<10} {'Direct (s)':<12} {'Mutex (s)':<12} {'WorkSteal (s)':<12} {'Speedup WS':<12}")
        print("-" * 60)
        
        for cities in sorted(results.keys()):
            r = results[cities]
            direct = r.get('direct', {}).get('mean', 0)
            mutex = r.get('mutex', {}).get('mean', 0)
            ws = r.get('worksteal', {}).get('mean', 0)
            speedup = direct / ws if ws > 0 else 0
            
            print(f"{cities:<10} {direct:<12.6f} {mutex:<12.6f} {ws:<12.6f} {speedup:<12.2f}x")
    
    def print_cutoff_results(self, results, cities):
        """Affiche les résultats du cutoff"""
        print("\nRésultats Impact Cutoff:")
        print(f"Cities: {cities}")
        print(f"{'Cutoff':<15} {'Direct (s)':<12} {'Mutex (s)':<12} {'WorkSteal (s)':<12}")
        print("-" * 55)
        
        for key in sorted(results.keys()):
            r = results[key]
            direct = r.get('direct', {}).get('mean', 0)
            mutex = r.get('mutex', {}).get('mean', 0)
            ws = r.get('worksteal', {}).get('mean', 0)
            
            print(f"{key:<15} {direct:<12.6f} {mutex:<12.6f} {ws:<12.6f}")
    
    def print_thread_results(self, results):
        """Affiche les résultats du scaling des threads"""
        print("\n Résultats Thread Scaling:")
        print(f"{'Threads':<10} {'Mutex (s)':<12} {'WorkSteal (s)':<12} {'Speedup M':<12} {'Speedup WS':<12}")
        print("-" * 60)
        
        base_mutex = results.get(1, {}).get('mutex', {}).get('mean', 1)
        base_ws = results.get(1, {}).get('worksteal', {}).get('mean', 1)
        
        for threads in sorted(results.keys()):
            r = results[threads]
            mutex = r.get('mutex', {}).get('mean', 0)
            ws = r.get('worksteal', {}).get('mean', 0)
            
            speedup_m = base_mutex / mutex if mutex > 0 else 0
            speedup_ws = base_ws / ws if ws > 0 else 0
            
            print(f"{threads:<10} {mutex:<12.6f} {ws:<12.6f} {speedup_m:<12.2f}x {speedup_ws:<12.2f}x")
    
    def print_speedup_results(self, results):
        """Affiche les résultats de speedup"""
        print("\n Résultats Speedup:")
        print(f"{'Cities':<10} {'Speedup Mutex':<15} {'Speedup WS':<15} {'Efficiency WS':<15}")
        print("-" * 60)
        
        for cities in sorted(results.keys()):
            r = results[cities]
            print(f"{cities:<10} {r['speedup_mutex']:<15.2f}x {r['speedup_ws']:<15.2f}x {r['efficiency_ws']:<15.2%}")
    
    def save_all_results(self):
        """Sauvegarde tous les résultats"""
        with open('benchmark_complete.json', 'w') as f:
            json.dump(self.all_results, f, indent=2)
        print(f"\n Tous les résultats sauvegardés dans benchmark_complete.json")
    
    def generate_report(self):
        """Génère un rapport texte complet"""
        with open('benchmark_report.txt', 'w') as f:
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
        
        print("Rapport généré dans benchmark_report.txt")


def main():
    print("TSP Complete Benchmark Suite")
    print("="*80)
    
    benchmark = CompleteBenchmark(executable="../tsp", num_runs=5)
    
    # Exécuter tous les tests
    benchmark.test_scalability()
    benchmark.test_cutoff_impact()
    benchmark.test_thread_scaling()
    benchmark.test_speedup()
    
    # Sauvegarder les résultats
    benchmark.save_all_results()
    benchmark.generate_report()
    
    print("\n Benchmarking complet terminé!")
    print("\nFichiers générés:")
    print("  - benchmark_complete.json (toutes les données)")
    print("  - benchmark_report.txt (rapport résumé)")


if __name__ == "__main__":
    main()
