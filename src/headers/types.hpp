#include "structs.hpp"
#include <queue>
#ifndef TYPES_H
#define TYPES_H

namespace scheduler
{

    typedef std::priority_queue<ProgramInfo, std::vector<ProgramInfo>, decltype(program_burst_compare)> burst_priority_queue;

    typedef std::priority_queue<ProgramInfo, std::vector<ProgramInfo>, decltype(program_priority_compare)> priority_queue;
}

#endif