// compiler avec:
// g++ -o testatom testatom.cpp

#include <iostream>

#include "atomicstamped.hpp"


int main()
{
	std::string sa = "a";
	AtomicStamped<std::string> ar(&sa, 10);

	uint64_t stamp;
	std::string* sp = ar.get(stamp);
	std::cout << "(start) string = " << *sp << " stamp = " << stamp << '\n';

	std::string sb = "b";
	bool c = ar.cas(&sa, &sb, 10, 12);
	sp = ar.get(stamp);
	std::cout << "(cas=" << c << ") string = " << *sp << " stamp = " << stamp << '\n';

	c = ar.cas(&sa, &sb, 10, 12);
	sp = ar.get(stamp);
	std::cout << "(cas=" << c << ") string = " << *sp << " stamp = " << stamp << '\n';

	return 0;
}
