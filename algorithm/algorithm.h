#ifndef	ALGORITHM_ALGORITHM_H
#define ALGORITHM_ALGORITHM_H

#include <vector>
#include <string>
#include "LFU.h"

class Algorithm{
private:

	int type;
	std::vector<unsigned long> hot_pages_;
	std::vector<unsigned long> cold_pages_;
	LFU *activeLFU;
	LFU *inactiveLFU;

public:
	Algorithm();
	Algorithm(int type);
	~Algorithm();
	void set_type(int type);
	int get_type();
	void set_activeLFU();
	void set_inactiveLFU();
	LFU *get_activeLFU();
	LFU *get_inactiveLFU();
};


#endif //ALGORITHM_ALGORITHM_H
