#include <iostream>
#include "tsptask.hpp"
#include "mutex_runner.hpp"
#include "work_stealing_runner.hpp"
/*****************************************************************
  Program to solve a TSP problem
  Arguments: tsp <filename> [number]
             filename: file to load the TSP graph from
             number: size of the graph (resized from the file)
  The program uses a TSPTask to solve the TSP problem, using
  two runners: DirectTaskRunner directly calls solve(), and
  PartitionedTaskStackRunner calls split(), then recurse in all
  partitions, then call merge().
 *****************************************************************/

int main(int argc, char** argv) {
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: " << argv[0] << " <file.tsp> [number]\n";
		return 1;
	}

	TSPGraph graph(argv[1]);
	if (argc == 3)
		graph.resize(atoi(argv[2]));

	TSPPath::setup(&graph);

	TSPTask tsp1;
	DirectTaskRunner r1;
	r1.run(&tsp1);
	std::cout << "direct: " << tsp1.result() << " t:" << r1.duration() << std::endl;

	TSPTask tsp2;
	tsp2.cutoff(0);
	MutexTaskRunner r2(std::thread::hardware_concurrency(), TSPPath::MAX_GRAPH);
	r2.run(&tsp2);
	std::cout << "mutex: " << tsp2.result() << " t:" << r2.duration() << std::endl;
	
	TSPTask tsp3;
	tsp3.cutoff(0);
	WorkStealingRunner r3(std::thread::hardware_concurrency(), TSPPath::MAX_GRAPH);
	r3.run(&tsp3);
	std::cout << "worksteal: " << tsp3.result() << " t:" << r3.duration() << std::endl;
	
	return 0;
}
