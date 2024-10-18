#include "pagenode.h"

#include <signal.h>
#include <numaif.h>

#define PAGE_SIZE                       4096

// one segment manage 128MB virtual space
#define SEGMENT_SIZE                    (128UL << 20)
#define ALIGN_TO_SEGMENT_START(addr)    ((addr) & (~(SEGMENT_SIZE - 1)))
// a 4K page is managed by a info of 4 bit, 128MB needs 16384B to manage
#define SEGMENT_INFO_SIZE               (SEGMENT_SIZE / PAGE_SIZE / 2)

// batch size to call move_pages()
#define GET_NUMA_NODE_BATCH_SIZE        512
// max count of numa node
#define MAX_NUMA_NODE                   12

PageNode::PageNode()
{
    m_pid = -1;
}

PageNode::~PageNode()
{
    unbind();
}

int PageNode::bind(pid_t pid)
{
    if(m_pid >= 0)
        ERROR({}, -EINVAL, false, "this PageNode has bound already");
    int ret = kill(pid, 0);
    if(ret)
        ERROR({}, -ESRCH, false, "no such process whose pid is %d", pid);
    m_pid = pid;
    return 0;
}

void PageNode::unbind()
{
    if(m_pid < 0)
        return;
    for(auto it = m_segments.begin(); it != m_segments.end(); ++it)
        delete it->info;
    m_segments.clear();
    m_pid = -1;
}

static void setInfo(uint8_t* info, size_t index, int status)
{
    assert((status & 0xf0) == 0);
    size_t byte_index = index / 2;
    int which = index % 2;
    uint8_t& byte = info[byte_index];
    // each info is 4 bits
    if(which == 0)
    {
        byte &= 0xf;
        byte |= status;
    }
    else
    {
        byte &= 0xf0;
        byte |= status << 4;
    }
}

static int getInfo(uint8_t* info, size_t index)
{
    size_t byte_index = index / 2;
    int which = index % 2;
    uint8_t byte = info[byte_index];
    if(which == 0)
        return byte & 0xf;
    else
        return (byte >> 4) & 0xf;
}

// init the <info> field of Segment
int PageNode::createInfo(Segment* segment)
{
    segment->info = new uint8_t[SEGMENT_INFO_SIZE];
    memset(segment->info, 0xff, SEGMENT_INFO_SIZE);
    size_t info_count = 0;
    void* address[GET_NUMA_NODE_BATCH_SIZE];
    int status[GET_NUMA_NODE_BATCH_SIZE];
    size_t batch_size = 0;
    uint64_t addr = segment->start;
    uint64_t end = addr + SEGMENT_SIZE;
    while(addr < end)
    {
        while(addr < end)
        {
            address[batch_size++] = (void*)addr;
            addr += PAGE_SIZE;
            if(batch_size == GET_NUMA_NODE_BATCH_SIZE)
                break;
        }
        int ret = move_pages(m_pid, batch_size, address, NULL, status, MPOL_MF_MOVE);
        if(ret)
        {
            ret = -errno;
            ERROR({}, ret, true,
                  "move_pages(%d, %lu, address, NULL, status, MPOL_MF_MOVE) failed: ",
                  m_pid, batch_size);
        }
        for(size_t i = 0; i < batch_size; i++)
        {
            int s = status[i];
            if(s < 0 || s >= MAX_NUMA_NODE)
                s = 0xf;
            setInfo(segment->info, info_count, s);
            info_count++;
        }
        batch_size = 0;
    }
    assert(info_count == SEGMENT_INFO_SIZE * 2);
    return 0;
}

int PageNode::getNode(uint64_t address)
{
    void* addr = (void*)address;
    int status;
    int ret = move_pages(m_pid, 1, &addr, NULL, &status, MPOL_MF_MOVE);
    if(ret)
    {
        ret = -errno;
        ERROR({}, ret, true,
              "move_pages(%d, 1, &addr, NULL, &status, MPOL_MF_MOVE) failed: ", m_pid);
    }
    if(status < 0 || status >= MAX_NUMA_NODE)
        status = 0xf;
    return status;
}

int PageNode::where(uint64_t address, bool force)
{
    if(m_pid < 0)
        ERROR({}, -EINVAL, false, "this PageNode has not been bound yet");
    Segment segment;
    // search segment by <start>
    segment.start = ALIGN_TO_SEGMENT_START(address);
    auto it = m_segments.find(segment);
    // if already in set, then get it
    if(it != m_segments.end())
        segment.info = it->info;
        // otherwise build the segment and insert it
    else
    {
        int ret = createInfo(&segment);
        if(ret)
            ERROR({}, ret, false, "createInfo(&segment) failed");
        m_segments.insert(segment);
    }
    assert(address >= segment.start);
    // index of byte of the info structure
    size_t index = (address - segment.start) / PAGE_SIZE;
    int status = getInfo(segment.info, index);
    if(status != 0xf)
        return status;
    if(!force)
        return 255;
    // get the current node of this page
    status = getNode(address);
    setInfo(segment.info, index, status);
    if(status == 0xf)
        return 255;
    return status;
}

int PageNode::invalidate(uint64_t start, uint64_t end)
{
    if(m_pid < 0)
        ERROR({}, -EINVAL, false, "this PageNode has not been bound yet");
    if(m_segments.size() == 0)
        return 0;
    Segment fake;
    fake.start = start;
    auto it = m_segments.lower_bound(fake);
    if(it != m_segments.begin())
    {
        auto prev = it;
        --prev;
        if(prev->start + SEGMENT_SIZE > start)
            it = prev;
    }
    assert(start < it->start + SEGMENT_SIZE);
    while(it != m_segments.end())
    {
        if(it->start >= end)
            break;
        delete it->info;
        m_segments.erase(it++);
    }
    return 0;
}