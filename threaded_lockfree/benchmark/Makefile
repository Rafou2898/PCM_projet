# Makefile for TSP Work-Stealing Project with Benchmarking

CXX = g++
CXXFLAGS = -std=c++20 -pthread -O3 -g -Wall -Wextra -fno-omit-frame-pointer
CXXFLAGS_DEBUG = -std=c++20 -pthread -O0 -g -Wall -Wextra

TARGET = ../tsp
TARGET_DEBUG = ../tsp_debug
SOURCES = ../tsp.cpp
HEADERS = ../tsptask.hpp ../mutex_runner.hpp ../work_stealing_runner.hpp ../work_stealing_deque.hpp ../task.hpp ../tspgraph.hpp

# Default target
all: $(TARGET)

# Release build (optimized for benchmarking)
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX)  $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	@echo "Release build complete: $(TARGET)"

# Debug build (with logs and debugging symbols)
debug: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS_DEBUG) $(SOURCES) -o $(TARGET_DEBUG)
	@echo "Debug build complete: $(TARGET_DEBUG)"

# Clean
clean:
	rm -f $(TARGET) $(TARGET_DEBUG) *.o
	@echo "🧹 Cleaned build artifacts"

# Clean all (including benchmark results)
cleanall: clean
	rm -f benchmark_*.json benchmark_*.csv benchmark_*.txt *.png
	@echo "🧹 Cleaned all generated files"

# Quick test
test: $(TARGET)
	@echo "🧪 Running quick test on dj05.tsp..."
	./$(TARGET) ../dj05.tsp --quiet

# Benchmark targets
.PHONY: bench bench-quick bench-full bench-scalability bench-cutoff bench-threads

# Quick benchmark (fast, for testing)
bench-quick: $(TARGET)
	@echo "🚀 Running quick benchmark..."
	@python3 -c "from benchmark_complete import *; b = CompleteBenchmark(num_runs=3); b.test_speedup(); b.save_all_results()"

# Full benchmark (all tests, 5 runs each)
bench-full: $(TARGET)
	@echo "🚀 Running full benchmark suite..."
	@python3 benchmark_complete.py

# Specific benchmarks
bench-scalability: $(TARGET)
	@echo "📈 Running scalability benchmark..."
	@python3 -c "from benchmark_complete import *; b = CompleteBenchmark(); b.test_scalability(); b.save_all_results()"

bench-cutoff: $(TARGET)
	@echo "✂️  Running cutoff impact benchmark..."
	@python3 -c "from benchmark_complete import *; b = CompleteBenchmark(); b.test_cutoff_impact(); b.save_all_results()"

bench-threads: $(TARGET)
	@echo "🧵 Running thread scaling benchmark..."
	@python3 -c "from benchmark_complete import *; b = CompleteBenchmark(); b.test_thread_scaling(); b.save_all_results()"

# Help
help:
	@echo "TSP Work-Stealing Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build release version (optimized)"
	@echo "  make debug        - Build debug version (with logs)"
	@echo "  make test         - Quick test on dj05.tsp"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make cleanall     - Clean everything including results"
	@echo ""
	@echo "Benchmarking:"
	@echo "  make bench-quick  - Quick benchmark (3 runs, speedup only)"
	@echo "  make bench-full   - Full benchmark suite (all tests)"
	@echo "  make bench-scalability - Test scalability only"
	@echo "  make bench-cutoff - Test cutoff impact only"
	@echo "  make bench-threads - Test thread scaling only"
	@echo ""
	@echo "Manual usage:"
	@echo "  ./tsp <file.tsp> [options]"
	@echo "  Options: --cities N, --threads N, --cutoff N, --no-cutoff, --quiet"

.PHONY: all debug clean cleanall test help
