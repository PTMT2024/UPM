#include "LFU.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <map>
#include <cmath>
#include <vector>

using namespace std;
// Constructor
LFU::LFU() {
  this->head = new FrequencyList<unsigned long>(0, NULL, NULL);
  this->tail = new FrequencyList<unsigned long>(0, NULL, NULL);
  this->CountFrequencyListMap[0] = this->head;
}

// Destructor
LFU::~LFU() {
  delete (this->head);
  this->NodeFrequencyListMap.clear();
  this->CountFrequencyListMap.clear();
}

// Adds an item to the cache with keeping the current access count
void LFU::Set(unsigned long node, int cur_cnt) {
  Evict(node);
  int freq = cur_cnt;
  if (FrequencyNodePresent(freq)) {
    FrequencyList<unsigned long>* frequencyList =
      this->CountFrequencyListMap.find(freq)->second;
    frequencyList->Add(node);
    this->NodeFrequencyListMap[node] = frequencyList;
  }
  else {
    FrequencyList<unsigned long>* newFrequencyList =
      new FrequencyList<unsigned long>(freq);
    newFrequencyList->Add(node);
    this->CountFrequencyListMap[freq] = newFrequencyList;
    this->NodeFrequencyListMap[node] = newFrequencyList;
    int prev_freq = 0;
    for (int cnt = freq - 1; cnt > 0; cnt--) {
      if (FrequencyNodePresent(cnt)) {
        prev_freq = cnt;
        break;
      }
    }
    if (prev_freq == 0) {
      newFrequencyList->SetPrevious(this->head);
      newFrequencyList->SetNext(this->head->GetNext());
      this->head->SetNext(newFrequencyList);
      if (newFrequencyList->GetNext() != NULL) {
        newFrequencyList->GetNext()->SetPrevious(newFrequencyList);
      }
      else {
        this->tail = newFrequencyList;
      }
      return;
    }
    FrequencyList<unsigned long>* prevFrequencyList =
      this->CountFrequencyListMap.find(prev_freq)->second;
    newFrequencyList->SetPrevious(prevFrequencyList);
    newFrequencyList->SetNext(prevFrequencyList->GetNext());
    prevFrequencyList->SetNext(newFrequencyList);
    if (newFrequencyList->GetNext() != NULL) {
      newFrequencyList->GetNext()->SetPrevious(newFrequencyList);
    }
    else {
      this->tail = newFrequencyList;
    }
  }
}

// Adds an item to the cache
void LFU::Set(unsigned long value) {
  unsigned long node = value;
  // if exists node
  if (NodePresent(node)) {
    std::unordered_map<unsigned long,
      FrequencyList<unsigned long>*>::const_iterator iter1 =
      this->NodeFrequencyListMap.find(node);
    FrequencyList<unsigned long>* frequencyList = iter1->second;
    int freq = frequencyList->GetCount();

    frequencyList->Delete(node);

    freq = freq + 1;

    // if this freq exists
    if (FrequencyNodePresent(freq)) {
      FrequencyList<unsigned long>* frequencyList1 =
        this->CountFrequencyListMap.find(freq)->second;
      // also add this node to this freq list
      frequencyList1->Add(node);
      // attach the same freq list to the node list
      this->NodeFrequencyListMap[node] = frequencyList1;
    }
    else {
      // if this freq not exists
      FrequencyList<unsigned long>* newFrequencyNode =
        new FrequencyList<unsigned long>(freq);
      newFrequencyNode->Add(node);
      this->NodeFrequencyListMap[node] = newFrequencyNode;
      this->CountFrequencyListMap[freq] = newFrequencyNode;

      newFrequencyNode->SetPrevious(frequencyList);
      newFrequencyNode->SetNext(frequencyList->GetNext());
      frequencyList->SetNext(newFrequencyNode);
      if (newFrequencyNode->GetNext() != nullptr) {
        newFrequencyNode->GetNext()->SetPrevious(newFrequencyNode);
      }
      else {
        this->tail = newFrequencyNode;
      }
    }

    // clean
    if (IsNodeEmpty(frequencyList)) {
      if (frequencyList->GetNext() != nullptr) {
        frequencyList->GetNext()->SetPrevious(frequencyList->GetPrevious());
      }
      else {
        this->tail = frequencyList->GetPrevious();
      }
      if (frequencyList->GetPrevious() != nullptr) {
        frequencyList->GetPrevious()->SetNext(frequencyList->GetNext());
      }
      this->CountFrequencyListMap.erase(frequencyList->GetCount());
      free(frequencyList);
    }
  }
  else {
    // not exists nodes
    int freq = 1;
    // if freq list has 1 freq
    if (FrequencyNodePresent(freq)) {
      FrequencyList<unsigned long>* frequencyList1 =
        this->CountFrequencyListMap.find(freq)->second;
      frequencyList1->Add(node);
      this->NodeFrequencyListMap[node] = frequencyList1;
    }
    else {
      FrequencyList<unsigned long>* newFrequencyNode =
        new FrequencyList<unsigned long>(freq);
      this->CountFrequencyListMap[freq] = newFrequencyNode;
      newFrequencyNode->SetPrevious(this->head);
      newFrequencyNode->SetNext(this->head->GetNext());
      newFrequencyNode->Add(node);

      this->head->SetNext(newFrequencyNode);
      if (newFrequencyNode->GetNext() != NULL) {
        newFrequencyNode->GetNext()->SetPrevious(newFrequencyNode);
      }
      else {
        this->tail = newFrequencyNode;
      }
      this->NodeFrequencyListMap[node] = newFrequencyNode;
    }
  }
}

// Retrieve a value from the cache
void LFU::Retrieve(unsigned long node) { Set(node); }

void LFU::CoolingDown() {
  auto start_time = std::chrono::steady_clock::now();
  std::cout << "CoolingDown Begin" << ::endl;

  FrequencyList<unsigned long>* list = this->head->GetNext();
  while (list != NULL) {
    int old_count = list->GetCount();
    // halved count of every freq list
    int new_count = std::ceil(old_count / 2.0);
    list->SetCount(new_count);
    // merge
    if (list->GetCount() == list->GetPrevious()->GetCount()) {
      FrequencyList<unsigned long>* prev = list->GetPrevious();
      const std::vector<unsigned long>& nodes = list->RetrieveKeysAsArray();
      for (unsigned long node : nodes) {
        prev->Add(node);
        this->NodeFrequencyListMap[node] = prev;
        list->Delete(node);
      }
      if (list->GetNext() != NULL) {
        list->GetNext()->SetPrevious(prev);
      }
      else {
        this->tail = prev;
      }
      prev->SetNext(list->GetNext());
      this->CountFrequencyListMap.erase(old_count);
      free(list);
      list = prev;
    }
    else {
      this->CountFrequencyListMap.erase(old_count);
      this->CountFrequencyListMap[list->GetCount()] = list;
    }
    list = list->GetNext();
  }
  auto end_time = std::chrono::steady_clock::now();
  auto time_duration = std::chrono::duration_cast<chrono::microseconds>(
    end_time - start_time)
    .count();
  std::cout << "CoolingDown End, duration: " << time_duration << " us." << std::endl;
}

// Remove node present in cache
int LFU::Evict(unsigned long value) {
  int freq = 0;
  unsigned long node = value;
  if (NodePresent(node)) {
    FrequencyList<unsigned long>* presentFrequencyNode =
      this->NodeFrequencyListMap[node];
    if (presentFrequencyNode == nullptr) {
      std::cout << "presentFrequencyNode is NULL" << std::endl;
      return freq;
    }
    freq = presentFrequencyNode->GetCount();
    presentFrequencyNode->Delete(node);
    // std::cout << "*** After presentFrequencyNode delete node *** " <<
    // std::endl;
    if (IsNodeEmpty(presentFrequencyNode)) {
      // std::cout << "*** Check IsNodeEmpty: presentFrequencyNode *** " <<
      // std::endl;
      if (presentFrequencyNode->GetNext() != NULL) {
        presentFrequencyNode->GetNext()->SetPrevious(
          presentFrequencyNode->GetPrevious());
      }
      else {
        this->tail = presentFrequencyNode->GetPrevious();
      }
      if (presentFrequencyNode->GetPrevious() != NULL)
        presentFrequencyNode->GetPrevious()->SetNext(
          presentFrequencyNode->GetNext());
      this->CountFrequencyListMap.erase(presentFrequencyNode->GetCount());
      this->NodeFrequencyListMap.erase(node);
      free(presentFrequencyNode);
    }
    else {
      // std::cout << "*** Before NodeFrequencyListMap erase node *** " <<
      // std::endl;
      this->NodeFrequencyListMap.erase(node);
      // std::cout << "*** After NodeFrequencyListMap erase node *** " <<
      // std::endl;
    }
  }
  // std::cout << "not found node: " << node << std::endl;
  return freq;
}

bool LFU::NodePresent(unsigned long node) {
  std::unordered_map<unsigned long,
    FrequencyList<unsigned long>*>::const_iterator iter1 =
    this->NodeFrequencyListMap.find(node);
  return !(iter1 == this->NodeFrequencyListMap.end());
}

bool LFU::FrequencyNodePresent(unsigned long count) {
  std::unordered_map<unsigned long,
    FrequencyList<unsigned long>*>::const_iterator iter1 =
    this->CountFrequencyListMap.find(count);
  return !(iter1 == this->CountFrequencyListMap.end());
}

bool LFU::IsNodeEmpty(FrequencyList<unsigned long>* frequencyList) {
  return frequencyList->IsEmpty();
}

void LFU::PrintLFU() {
  FrequencyList<unsigned long>* Node = this->head;
  while (Node != NULL) {
    Node->PrintNodeList();
    Node = Node->GetNext();
  }
}

bool LFU::isNodePresent(unsigned long node) { return NodePresent(node); }

int LFU::CountNodeFreq(unsigned long node) {
  // if exists node
  if (NodePresent(node)) {
    std::unordered_map<unsigned long,
      FrequencyList<unsigned long>*>::const_iterator iter1 =
      this->NodeFrequencyListMap.find(node);
    FrequencyList<unsigned long>* frequencyList = iter1->second;
    return frequencyList->GetCount();
  }
  else {
    return 0;
  }
}

uint32_t LFU::PrintLFUCnt() {
  uint32_t total_cnt = 0;
  FrequencyList<unsigned long>* Node = this->head;
  while (Node != NULL) {
    total_cnt += Node->PrintNodeListCnt();
    Node = Node->GetNext();
  }
  return total_cnt;
}


void LFU::SetListValueToZero() {
  // reset countFreq to 0
  if (!this->NodeFrequencyListMap.empty()) {
    for (auto& it : this->NodeFrequencyListMap) {
      FrequencyList<unsigned long>* presentFrequencyNode = it.second;
      this->CountFrequencyListMap.at(presentFrequencyNode->GetCount())
        ->Delete(it.first);
      it.second = new FrequencyList<unsigned long>(0);
    }
  }
}

// The first API reports which pages will be migrated from the slow memory to
// the fast memory
//  (in other words, which pages will be migrated from the inactive list to
//  the active list).
void LFU::get_hot_pages(std::vector<unsigned long>& hot_pages,
  unsigned long hotPageLimit, LFU* active) {
  auto lfu_start_clock = std::chrono::steady_clock::now();
  hot_pages.clear();
  if (this->CountFrequencyListMap.empty()) {
    return;
  }
  FrequencyList<unsigned long>* list = this->tail;
  while (list != this->head && list != NULL) {
    if (list->GetCount() < hotPageLimit) {
      break;
    }
    const std::vector<unsigned long>& nodes = list->RetrieveKeysAsArray();
    for (unsigned long node : nodes) {
      hot_pages.push_back(node);
    }
    list = list->GetPrevious();
  }

  auto lfu_end_clock = std::chrono::steady_clock::now();
  auto lfu_total_time = chrono::duration_cast<chrono::microseconds>(
    lfu_end_clock - lfu_start_clock)
    .count();
  std::cout << "++++++++++++ Movepage LFU get_hot_pages total time: "
    << lfu_total_time << ", hot pages size: " << hot_pages.size()
    << std::endl;
}

// The first API reports which pages will be migrated from the slow memory to
// the fast memory
//  (in other words, which pages will be migrated from the inactive list to
//  the active list).
void LFU::get_hot_pages_old(std::vector<unsigned long>& hot_pages,
  unsigned long hotPageLimit, LFU* active,
  int radius) {
  // find from inactiveList
  // std::cout << "clear before: " << hot_pages.size() << std::endl;
  auto lfu_start_clock = std::chrono::steady_clock::now();
  hot_pages.clear();
  // std::cout << "clear after: " << hot_pages.size() << std::endl;
  if (this->CountFrequencyListMap.empty()) {
    return;
  }
  int real_hotpage_cnt = 0;
  // std::cout << "count node list map size:: " <<
  // this->NodeFrequencyListMap.size() << std::endl;
  for (auto kv : this->CountFrequencyListMap) {
    if (kv.first >= hotPageLimit) {
      const std::vector<unsigned long>& nodes =
        kv.second->RetrieveKeysAsArray();
      for (unsigned long node : nodes) {
        real_hotpage_cnt++;
        if (radius == 0) {
          hot_pages.push_back(node);
          // active->Set(node, kv.first);
        }
        else {
          for (int i = -1 * radius; i < radius; i++) {
            if (node + i >= 0) {
              hot_pages.push_back(node + i);
              // active->Set(node + i, kv.first);
            }
          }
        }
      }
    }
  }
  // TODO  optimize
  std::reverse(hot_pages.begin(), hot_pages.end());
  auto lfu_end_clock = std::chrono::steady_clock::now();
  auto lfu_total_time = chrono::duration_cast<chrono::microseconds>(
    lfu_end_clock - lfu_start_clock)
    .count();
  std::cout << "++++++++++++ Movepage LFU get_hot_pages total time: "
    << lfu_total_time << ", real_hotpage_cnt: " << real_hotpage_cnt
    << ", hot pages size: " << hot_pages.size() << std::endl;
}

// The second API reports which pages will be migrated from the fast memory to
// the slow memory
//  (in other words, which pages will be migrated from the active list to the
//  inactive list).

void LFU::get_cold_pages(std::vector<unsigned long>& cold_pages,
  unsigned long request_num, int hot_page_threshold) {
  auto lfu_start_clock = std::chrono::steady_clock::now();
  if (request_num <= 0) {
    return;
  }
  cold_pages.clear();
  if (this->CountFrequencyListMap.empty()) {
    return;
  }

  // hot_page_threshold = hot_page_threshold - 1;
  // if (hot_page_threshold < 2) {
  //   hot_page_threshold = 2;
  // }

  FrequencyList<unsigned long>* list = this->head->GetNext();
  while (list != NULL) {
    if (list->GetCount() >= hot_page_threshold) {
      break;
    }
    const std::vector<unsigned long>& nodes = list->RetrieveKeysAsArray();
    for (unsigned long node : nodes) {
      cold_pages.push_back(node);
      if (cold_pages.size() >= request_num) {
        break;
      }
    }
    if (cold_pages.size() >= request_num) {
      break;
    }
    list = list->GetNext();
  }

  auto lfu_end_clock = std::chrono::steady_clock::now();
  auto lfu_total_time = chrono::duration_cast<chrono::microseconds>(
    lfu_end_clock - lfu_start_clock)
    .count();
  bool get_engough_request_num = (cold_pages.size() >= request_num);
  std::cout << "++++++++++++ Movepage LFU get_cold_pages total time: "
    << lfu_total_time << ", cold pages size: " << cold_pages.size()
    << ", request_num: " << request_num << ", get_engough_request_num: " << get_engough_request_num
    << std::endl;


  // for (auto kv : this->CountFrequencyListMap) {
  //   const std::vector<unsigned long>& nodes =
  //     kv.second->RetrieveKeysAsArray();
  //   std::cout << "[Movepage] get_cold_pages access_cnt: " << kv.first << " page_count: "
  //     << nodes.size() << " hot_threshold: " << hot_page_threshold << std::endl;
  //   if (kv.first < hot_page_threshold) {
  //     for (unsigned long node : nodes) {
  //       cold_pages.push_back(node);
  //       if (cold_pages.size() >= request_num) {
  //         return;
  //       }
  //     }
  //   }
  // }

  return;
}

void LFU::get_cold_pages_old(std::vector<unsigned long>& cold_pages,
  unsigned long request_num,
  int min_freq_lower_bound) {
  // if meet the request num, end
  if (request_num <= 0) {
    return;
  }
  if (min_freq_lower_bound == -1) {
    // the first time
    // used to remove all the elements of the vector container, thus making it
    // size 0.
    cold_pages.clear();
    if (this->NodeFrequencyListMap.empty()) {
      return;
    }
  }
  // if request num >= contained pages, then return all pages
  if (this->NodeFrequencyListMap.size() <= request_num) {
    for (auto kv : this->NodeFrequencyListMap) {
      cold_pages.push_back(kv.first);
    }
    return;
  }
  // else request num < contained pages
  // find from activeList
  int minFreq = INT_MAX;
  for (auto kv : this->CountFrequencyListMap) {
    if (min_freq_lower_bound != -1 && kv.first <= min_freq_lower_bound) {
      // filter out
      continue;
    }
    if (kv.first < minFreq) {
      minFreq = kv.first;
    }
  }
  // the nodes in this list fit for cold pages condition
  const std::vector<unsigned long>& nodes =
    this->CountFrequencyListMap.at(minFreq)->RetrieveKeysAsArray();
  for (int i = 0; i < nodes.size(); i++) {
    cold_pages.push_back(nodes[i]);
    if (cold_pages.size() >= request_num) {
      return;
    }
  }
  get_cold_pages_old(cold_pages, request_num, minFreq);
}

// return an ordered list of pages
void LFU::list_pages_by_freq(std::vector<unsigned long>& list_pages,
  bool reversed) {
  // default from low to high freq
  std::map<unsigned long, FrequencyList<unsigned long>*> wave;
  for (auto kv : this->CountFrequencyListMap) {
    wave.insert(std::pair<unsigned long, FrequencyList<unsigned long> *>(
      kv.first, kv.second));
  }
  if (!reversed) {
    for (auto& it : wave) {
      const std::vector<unsigned long>& nodes =
        it.second->RetrieveKeysAsArray();
      std::copy(nodes.begin(), nodes.end(), std::back_inserter(list_pages));
    }
  }
  else {
    for (auto it = wave.rbegin(); it != wave.rend(); it++) {
      const std::vector<unsigned long>& nodes =
        it->second->RetrieveKeysAsArray();
      std::copy(nodes.begin(), nodes.end(), std::back_inserter(list_pages));
    }
  }
}
