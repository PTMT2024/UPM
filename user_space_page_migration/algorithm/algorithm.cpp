#include "algorithm.h"
#include "LFU.h"
#include <vector>
#include <string>

// Constructor
Algorithm::Algorithm(){}

Algorithm::Algorithm(int type){
	set_type(type);
}

void Algorithm::set_type(int type){
	this->type = type;
}

int Algorithm::get_type(){
	return this->type;
}

void Algorithm::set_activeLFU(){
	this->activeLFU = new LFU();
}

LFU * Algorithm::get_activeLFU(){
	return this->activeLFU;
}

void Algorithm::set_inactiveLFU(){
	this->inactiveLFU = new LFU();
}

LFU * Algorithm::get_inactiveLFU(){
	return this->inactiveLFU;
}


