#include "FrequencyList.h"
#include <vector>

// Constructors
template<typename T>
FrequencyList<T>::FrequencyList() {

}

template<typename T>
FrequencyList<T>::FrequencyList(int count) {
    this->count = count;
}

template<typename T>
FrequencyList<T>::FrequencyList(int count, FrequencyList *left, FrequencyList *right) {
    this->count = count;
    this->left = left;
    this->right = right;
}

// Destructors

template<typename T>
FrequencyList<T>::~FrequencyList() {
    // // this will cause a stack overflow
    // delete (this->right);
    // delete (this->left);
    // this->nodelist.clear();

    if (this->right) {
        this->right->left = nullptr;  // Prevent back-reference
        delete this->right;
        this->right = nullptr;
    }
    
    if (this->left) {
        this->left->right = nullptr;  // Prevent back-reference
        delete this->left;
        this->left = nullptr;
    }

    this->nodelist.clear();
}


// Support functions
template<typename T>
void FrequencyList<T>::Add(T node) {
    this->nodelist[node] = 1;
}

template<typename T>
void FrequencyList<T>::Delete(T node) {
    if (IsPresent(node)) {
        // node present.
        this->nodelist.erase(node);
    }
}

template<typename T>
void FrequencyList<T>::PrintNodeList() {
    std::unordered_map<unsigned long, int>::const_iterator iter = this->nodelist.begin();
    while (iter != this->nodelist.end()) {
        std::cout << iter->first << " ";
        iter++;
    }
}

template<typename T>
int FrequencyList<T>::PrintNodeListCnt() {
    std::cout << this->count << ": ";
    std::cout << "Node list count: " << this->nodelist.size() << std::endl;
    return this->nodelist.size();
}

template<typename T>
bool FrequencyList<T>::IsPresent(T node) {
    std::unordered_map<unsigned long, int>::const_iterator iter = this->nodelist.find(node);
    return !(iter == this->nodelist.end());
}

template<typename T>
int FrequencyList<T>::GetCount() {
    return this->count;
}

template<typename T>
void FrequencyList<T>::SetCount(int count) {
    this->count = count;
}

template<typename T>
void FrequencyList<T>::SetNext(FrequencyList *right) {
    this->right = right;
}

template<typename T>
void FrequencyList<T>::SetPrevious(FrequencyList *left) {
    this->left = left;
}

template<typename T>
FrequencyList<T> *FrequencyList<T>::GetNext() {
    return this->right;
}

template<typename T>
FrequencyList<T> *FrequencyList<T>::GetPrevious() {
    return this->left;
}

template<typename T>
std::vector<unsigned long> FrequencyList<T>::RetrieveKeysAsArray() {
    std::vector<unsigned long> keys;
    for (auto kv : this->nodelist) {
        keys.push_back(kv.first);
    }
    return keys;
}

template<typename T>
bool FrequencyList<T>::IsEmpty() {
    return this->nodelist.empty();
}


template
class FrequencyList<unsigned long>;