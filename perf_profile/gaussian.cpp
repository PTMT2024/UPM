#include "stdio.h"
#include "stdlib.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <cmath>
#include <chrono>
#include <ctime>

extern "C" {
#include "profile.h"
}

using namespace std;

int main() {
    long* p;
    long* q;
    long* r;
    long tmp;
    //long size = 20480000000;
	long size = 81920000000;
    //long iteration = 500000000;
    long iteration = 900000000;

    p = (long *)malloc(size*sizeof(long));

    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<> d{40960000000, 4000000000};
	//std::normal_distribution<> d{10240000000, 10000000};
    //std::normal_distribution<> d{5120000, 400000};
    std::cout << &p[0] << std::endl;
    std::cout << &p[size - 1] << std::endl;
    //std::chrono::time_point<std::chrono::system_clock> start, end;

	auto start = std::chrono::steady_clock::now();
    pebs_init();
	for (long i = 0; i < iteration; i++) {
        long index = (long)std::round(d(gen));
        tmp = ++p[index];
    }
	pebs_shutdown();
    auto end = std::chrono::steady_clock::now();
	
	std::cout << "Elapsed time in nanoseconds: "
	          << chrono::duration_cast<chrono::nanoseconds>(end - start).count()
			  << " ns" << endl;

	std::cout << "Elapsed time in microseconds: "
	          << chrono::duration_cast<chrono::microseconds>(end - start).count()
		      << " µs" << endl;

	std::cout << "Elapsed time in milliseconds: "
	          << chrono::duration_cast<chrono::milliseconds>(end - start).count()
		      << " ms" << endl;

	std::cout << "Elapsed time in seconds: "
	          << chrono::duration_cast<chrono::seconds>(end - start).count()
		      << " sec" << endl;
    
	return 0;
}
