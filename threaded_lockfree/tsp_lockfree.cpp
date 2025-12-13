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

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 4)
	{
		std::cerr << "Usage: " << argv[0] << " <file.tsp> [number] [num_threads]\n";
		return 1;
	}

	TSPGraph graph(argv[1]);
	if (argc >= 3)
		graph.resize(atoi(argv[2]));

	// Determine number of threads from optional 3rd argument; fallback to hardware concurrency
	int number_threads = (argc == 4) ? std::max(1, atoi(argv[3])) : (int)std::thread::hardware_concurrency();

	TSPPath::setup(&graph);
	//cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << std::endl;
	//TSPTask tsp1;
	//DirectTaskRunner r1;
	//r1.run(&tsp1);
	//std::cout << "direct: " << tsp1.result() << " t:" << r1.duration() << std::endl;
	//TSPTask tsp2;
	//tsp2.cutoff(5);
	//MutexTaskRunner r2(number_threads, TSPPath::MAX_GRAPH);
	//cout << "Number of threads: " << number_threads << std::endl;
	//// MutexTaskRunner r2(std::thread::hardware_concurrency(), TSPPath::MAX_GRAPH);
	//r2.run(&tsp2);
	//std::cout << "mutex: " << tsp2.result() << " t:" << r2.duration() << std::endl;

	TSPTask tsp3;
	tsp3.cutoff(5);
	WorkStealingRunner r3(number_threads, TSPPath::MAX_GRAPH);
	cout << "Number of threads: " << number_threads << std::endl;
	// WorkStealingRunner r3(std::thread::hardware_concurrency(), TSPPath::MAX_GRAPH);
	r3.run(&tsp3);
	std::cout << "worksteal: " << tsp3.result() << " t:" << r3.duration() << std::endl;

	return 0;
}
