# Makefile for TSP Work-Stealing Project with Benchmarking

CXX = g++
CXXFLAGS = -std=c++20 -pthread -O3 -g -Wall -Wextra -fno-omit-frame-pointer

TARGET = ./tsp
SOURCES = ./tsp.cpp
HEADERS = ./tsptask.hpp ./mutex_runner.hpp ./mutex_runner_factorial.hpp ./work_stealing_runner.hpp ./work_stealing_deque.hpp ./task.hpp ./tspgraph.hpp

# Default target
all: $(TARGET)

# Release build (optimized for benchmarking)
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX)  $(CXXFLAGS) $(SOURCES) -o $(TARGET)
	@echo "Release build complete: $(TARGET)"

# Clean
clean:
	rm -f $(TARGET) $(TARGET_DEBUG) *.o
	@echo " Cleaned build artifacts"

# Run: compile and execute benchmarks
test: $(TARGET) bench-full
	@echo "✓ Compilation and benchmarks completed"

# Benchmark targets
.PHONY: test bench-full bench-scalability bench-cutoff bench-threads help clean
# Full benchmark (all tests, 5 runs each)
bench-full:
	@echo " Running full benchmark suite..."
	@python3 benchmark_complete.py --test all

# Specific benchmarks
bench-scalability:
	@echo " Running scalability benchmark..."
	@python3 benchmark_complete.py --test scalability

bench-cutoff:
	@echo "  Running cutoff impact benchmark..."
	@python3 benchmark_complete.py --test cutoff

bench-threads:
	@echo " Running thread scaling benchmark..."
	@python3 benchmark_complete.py --test threads

# Help
help:
	@echo "TSP Work-Stealing Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build release version (optimized)"
	@echo "  make test          - Compile and run full benchmark suite"
	@echo "  make clean        - Clean build artifacts"
	@echo ""
	@echo "Benchmarking:"
	@echo "  make bench-full   - Full benchmark suite (scalability, cutoff, threads)"
	@echo "  make bench-scalability - Test scalability only"
	@echo "  make bench-cutoff - Test cutoff impact only"
	@echo "  make bench-threads - Test thread scaling only"
	@echo ""
	@echo "Python script usage (direct calls):"
	@echo "  python3 benchmark_complete.py [OPTIONS]"
	@echo ""
	@echo "Options:"
	@echo "  --test [scalability|cutoff|threads|all] - Choose test type (default: all)"
	@echo "  --runs N                  - Number of runs per test (default: 5)"
	@echo "  --threads N               - Default thread count (default: 60)"
	@echo "  --cutoff N                - Default cutoff value (default: 8)"
	@echo "  --cities N                - Number of cities for cutoff/threads tests"
	@echo "  --skip-direct             - Skip direct method"
	@echo "  --skip-mutex              - Skip mutex method"
	@echo "  --skip-mutex-factorial    - Skip mutex factorial method"
	@echo "  --skip-worksteal          - Skip worksteal method"
	@echo "  --cities-range        - Range: 2-17 or 2,5,10,15"
	@echo "  --cutoff-values       - Range: 1-13 or 1,5,10"
	@echo "  --thread-counts       - Range: 1-256 or 1,10,50,100"
	@echo ""
	@echo "Manual usage:"
	@echo "  ./tsp <file.tsp> [options]"
	@echo "  Options: --cities N, --threads N, --cutoff N, --no-cutoff"

.PHONY: all clean test help
