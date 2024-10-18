#ifndef LFU_FREQUENCYLIST_H
#define LFU_FREQUENCYLIST_H

#include <iostream>
#include <unordered_map>
#include <vector>


template<typename T>
class FrequencyList {
private:
    int count;
    std::unordered_map<unsigned long, int> nodelist;
    FrequencyList *left;
    FrequencyList *right;
public:
    FrequencyList();

    FrequencyList(int count);

    FrequencyList(int count, FrequencyList *left, FrequencyList *right);

    ~FrequencyList();

    void Add(T node);

    void Delete(T node);

    bool IsPresent(T node);

    void PrintNodeList();

    int PrintNodeListCnt();

    int GetCount();
    void SetCount(int count);

    void SetPrevious(FrequencyList *left);

    void SetNext(FrequencyList *right);

    FrequencyList *GetPrevious();

    FrequencyList *GetNext();

    bool IsEmpty();

    std::vector<unsigned long> RetrieveKeysAsArray();
};

#endif //LFU_FREQUENCYLIST_H

