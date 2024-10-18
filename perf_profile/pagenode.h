#ifndef PAGENODE_H
#define PAGENODE_H

#include "common.h"

#include <set>
#include <unistd.h>

class PageNode
{
public:
    PageNode();

    ~PageNode();

    /* Initialized the PageNode to bind to a process.
     *      pid: the PID of target process
     * RETURN: 0 if ok, or a negative error code
     */
    int bind(pid_t pid);

    /* Uninitialized the PageNode.
     */
    void unbind();

    /* Get the NUMA node id where the page resides.
     *      address: the virtual address of the page
     *      force: if the cached node is '-1', then re-fetch the node information
     * RETURN: the node id, or 255 if page is unavailable, or a negetive error code
     */
    int where(uint64_t address, bool force = false);

    /* Invalidate the cached information.
     *      start: the start address
     *      end: the end address
     * RETURN: 0 if ok, or a negative error
     * NOTE: the cached information in range [start, end) is cleared
     */
    int invalidate(uint64_t start = 0, uint64_t end = 0xffffffffffffffffUL);

private:

    struct Segment
    {
        uint64_t start;
        uint8_t* info;

        bool operator <(const Segment& segment) const
        {
            return start < segment.start;
        }
    };

    int createInfo(Segment* segment);

    int getNode(uint64_t address);

private:
    pid_t m_pid;
    std::set<Segment> m_segments;
};


#endif
