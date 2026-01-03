#include <iostream>
#include <cstdlib>
#include <cstring>
#include "tsptask.hpp"
#include "mutex_runner.hpp"
#include "work_stealing_runner.hpp"

/*****************************************************************
  Program to solve a TSP problem with benchmarking capabilities
  
  Usage: tsp <file.tsp> [options]
  
  Options:
    --cities N          : Resize graph to N cities
    --threads N         : Number of threads (default: hardware_concurrency)
    --cutoff N          : Cutoff value (default: 5)
    --no-cutoff         : Disable cutoff
    --skip-direct       : Skip direct runner
    --skip-mutex        : Skip mutex runner
    --skip-worksteal    : Skip work-stealing runner
    --quiet             : Minimal output (only results)
 *****************************************************************/

struct Config {
    const char* filename = nullptr;
    int num_cities = -1;  // -1 = use file size
    int num_threads = -1; // -1 = use hardware_concurrency
    int cutoff = 5;
    bool use_cutoff = true;
    bool run_direct = true;
    bool run_mutex = true;
    bool run_worksteal = true;
    bool run_partitioned = true;
    bool quiet = false;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file.tsp> [options]\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --cities N      : Resize graph to N cities\n";
    std::cerr << "  --threads N     : Number of threads\n";
    std::cerr << "  --cutoff N      : Cutoff value (default: 5)\n";
    std::cerr << "  --no-cutoff     : Disable cutoff\n";
    std::cerr << "  --skip-direct   : Skip direct runner\n";
    std::cerr << "  --skip-mutex    : Skip mutex runner\n";
    std::cerr << "  --skip-worksteal: Skip work-stealing runner\n";
    std::cerr << "  --quiet         : Minimal output\n";
}

Config parse_args(int argc, char** argv) {
    Config config;
    
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    
    config.filename = argv[1];
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--cities" && i + 1 < argc) {
            config.num_cities = atoi(argv[++i]);
        }
        else if (arg == "--threads" && i + 1 < argc) {
            config.num_threads = atoi(argv[++i]);
        }
        else if (arg == "--cutoff" && i + 1 < argc) {
            config.cutoff = atoi(argv[++i]);
        }
        else if (arg == "--no-cutoff") {
            config.use_cutoff = false;
        }
        else if (arg == "--skip-direct") {
            config.run_direct = false;
        }
        else if (arg == "--skip-mutex") {
            config.run_mutex = false;
        }
        else if (arg == "--skip-partitioned") {
            config.run_partitioned = false;
        }
        else if (arg == "--skip-worksteal") {
            config.run_worksteal = false;
        }
        else if (arg == "--quiet" || arg == "-q") {
            config.quiet = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

int main(int argc, char** argv) {
    Config config = parse_args(argc, argv);
    
    // Load graph
    TSPGraph graph(config.filename);
    if (config.num_cities > 0) {
        graph.resize(config.num_cities);
    }
    
    TSPPath::setup(&graph);
    
    // Determine number of threads
    int num_threads = config.num_threads > 0 
                     ? config.num_threads 
                     : std::thread::hardware_concurrency();
    
    if (!config.quiet) {
        std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << std::endl;
        std::cout << "Graph size: " << graph.size() << " cities" << std::endl;
        std::cout << "Using " << num_threads << " threads" << std::endl;
        if (config.use_cutoff) {
            std::cout << "Cutoff: " << config.cutoff << std::endl;
        } else {
            std::cout << "Cutoff: 1" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Run direct (single-threaded)
    if (config.run_direct) {
        std::cout << "Running direct (single-threaded)..." << std::endl;
        TSPTask tsp1;
        DirectTaskRunner r1;
        r1.run(&tsp1);
        std::cout << "direct: " << tsp1.result() << " t:" << r1.duration() << std::endl;
    }
    
    // Run mutex-based parallel
    if (config.run_mutex) {
        std::cout << "Running mutex-based parallel..." << std::endl;
        TSPTask tsp2;
        if (config.use_cutoff) {
            tsp2.cutoff(config.cutoff);
        } else {
            tsp2.cutoff(1);
        }
        
        MutexTaskRunner r2(num_threads, TSPPath::MAX_GRAPH);
        if (!config.quiet) {
            std::cout << "Number of threads: " << num_threads << std::endl;
        }
        r2.run(&tsp2);
        std::cout << "mutex: " << tsp2.result() << " t:" << r2.duration() << std::endl;
    }
    
    // Run work-stealing parallel
    if (config.run_worksteal) {
        std::cout << "Running work-stealing parallel..." << std::endl;
        TSPTask tsp3;
        if (config.use_cutoff) {
            tsp3.cutoff(config.cutoff);
        }
        
        WorkStealingRunner r3(num_threads, TSPPath::MAX_GRAPH);
        if (!config.quiet) {
            std::cout << "Number of threads: " << num_threads << std::endl;
        }
        r3.run(&tsp3);
        std::cout << "worksteal: " << tsp3.result() << " t:" << r3.duration() << std::endl;
    } 
    // Run work-stealing parallel
    /* if (config.run_partitioned) {
        std::cout << "Running partitioned task..." << std::endl;
        TSPTask tsp4;
        if (config.use_cutoff) {
            tsp4.cutoff(config.cutoff);
        }
        
        PartitionedTaskStackRunner r4(TSPPath::MAX_GRAPH);
        r4.run(&tsp4);
        std::cout << "partitioned task: " << tsp4.result() << " t:" << r4.duration() << std::endl;
    } */
    
    return 0;
}
