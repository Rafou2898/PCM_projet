//
//  compiler avec g++ -std=c++11 -o testque testque.cpp
//

#include <iostream>
#include <thread>
#include <vector>
#include "queue.hpp"

struct Blob {
	int n;
	std::string s;
	Blob(int i) { static std::string tabs[] = { "blah", "bleh", "blih" }; n = i; s = tabs[i%3]; }
	Blob() { ; }
	Blob operator+(const Blob b) { return Blob(this->n + b.n); }
};

std::ostream &operator<<(std::ostream &os, Blob const &b)
{ 
	return os << b.n << ':' << b.s;
}


template <class T>
T sumup(Queue<T>* q, bool print = false)
{
	T ret(0);
	bool didonce = false;

	while (true) {
		try {
			T val = q->dequeue();
			if (print)
				std::cout << (didonce ? ", " : "") << val;
			ret = (ret + val);
			didonce = true;
		}
		catch (EmptyQueueException e) {
			break;
		}
	}
	if (print) {
		if (didonce)
			std::cout << " --- total: " << ret << '\n';
		else
			std::cout << "empty queue!\n";
	}

	return ret;
}

template <class T>
void test(Queue<T>* q, int max)
{
	for (int i=1; i<max; i++)
		q->enqueue(i);

	T sum = sumup(q);
	q->enqueue(sum);
}

template <class T>
void testthread()
{
	Queue<T> q;

	std::vector<std::thread> threads;
	for (int i = 0; i < 20; i++)
		threads.push_back(std::thread(test<T>, &q, 5));

	for (auto &th : threads)
		th.join();

	sumup(&q, true);
}

int main()
{
	testthread<int>();
	testthread<Blob>();
	return 0;
}