#ifndef LFU_LFU_H
#define LFU_LFU_H

#include "FrequencyList.h"
#include <vector>

class LFU {
private:

    // k,v - node, list contains this node
    std::unordered_map<unsigned long, FrequencyList<unsigned long> *> NodeFrequencyListMap;

    // k,v - frequency, node list
    std::unordered_map<unsigned long, FrequencyList<unsigned long> *> CountFrequencyListMap;
    FrequencyList<unsigned long> *head;
    FrequencyList<unsigned long> *tail;

    void Set(unsigned long value);

    bool NodePresent(unsigned long node);

    bool FrequencyNodePresent(unsigned long count);

    bool IsNodeEmpty(FrequencyList<unsigned long> *frequencyList);

public:
    LFU();

    ~LFU();

    void Retrieve(unsigned long value);

    void CoolingDown();

    void Set(unsigned long value, int cur_cnt);

    int Evict(unsigned long value);

    void PrintLFU();

    uint32_t PrintLFUCnt();

    bool isNodePresent(unsigned long node);

    int CountNodeFreq(unsigned long node);

    void SetListValueToZero();

    //The first API reports which pages will be migrated from the slow memory to the fast memory (in other words, which pages will be migrated from the inactive list to the active list).
    void get_hot_pages_old(std::vector<unsigned long>& hot_pages, unsigned long hotPageLimit, LFU *active, int radius);

    void get_hot_pages(std::vector<unsigned long>& hot_pages, unsigned long hotPageLimit, LFU *active);

    //The second API reports which pages will be migrated from the fast memory to the slow memory (in other words, which pages will be migrated from the active list to the inactive list).
    void get_cold_pages_old(std::vector<unsigned long>& cold_pages, unsigned long request_num, int min_freq_lower_bound = -1);
    void get_cold_pages(std::vector<unsigned long>& cold_pages, unsigned long request_num, int hot_page_threshold);

    void list_pages_by_freq(std::vector<unsigned long>& list_pages, bool reversed = false);
};


#endif //LFU_LFU_H
