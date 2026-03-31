#include <algorithm>
#include "algo.hpp"

std::vector<ExecutionBlock> FirstComeFirstServer::run(std::vector<ProgramInfo> programs)
{
    current_time = 0;
    std::sort(programs.begin(), programs.end(), [](ProgramInfo &a, ProgramInfo &b)
              { return a.arrival < b.arrival; });

    std::vector<ExecutionBlock> exection_blocks;
    for (const ProgramInfo &i : programs)
    {
        if (current_time < i.arrival)
        {
            ExecutionBlock idle = {0, i.arrival - current_time};
            exection_blocks.push_back(idle);

            current_time = i.arrival;
        }

        exection_blocks.push_back(ExecutionBlock{i.pid, i.burst});
        current_time += i.burst;
    }
    return exection_blocks;
}