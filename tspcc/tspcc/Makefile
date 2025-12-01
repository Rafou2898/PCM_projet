#  Copyright (c) 2012 Marcelo Pasin. All rights reserved.

CFLAGS=-O3 -Wall --std=c++20
LDFLAGS=-O3 -lm

all: tspcc

tspcc: tspcc.o
	c++ -o tspcc $(LDFLAGS) tspcc.o

tspcc.o: tspcc.cpp graph.hpp path.hpp tspfile.hpp
	c++ $(CFLAGS) -c tspcc.cpp

testatom: testatom.cpp atomicstamped.hpp
	g++ $(CFLAGS) -o testatom testatom.cpp

testque: testque.cpp queue.hpp
	g++ $(CFLAGS) -o testque testque.cpp

omp:
	make tspcc CFLAGS="-fopenmp -O3" LDFLAGS="-fopenmp -O3"

clean:
	rm -f *.o tspcc atomic testatom omp testque
