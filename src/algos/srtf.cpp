#include <vector>
#include <structs.hpp>
#include <algo.hpp>
#include <algorithm>
#include <unordered_map>

std::vector<ExecutionBlock> ShortestRemainingTimeFirst::run(std::vector<ProgramInfo> programs)
{
    current_time = 0;
    std::sort(programs.begin(), programs.end(), [](ProgramInfo &a, ProgramInfo &b)
              { return a.arrival < b.arrival; });

    std::unordered_map<size_t, size_t> remaining;
    for (ProgramInfo &p : programs)
        remaining[p.pid] = p.burst;

    std::vector<bool> visited(programs.size(), false);
    std::vector<ExecutionBlock> execution_blocks;
    size_t completed_program_count = 0;
    current_time = 0;

    while (completed_program_count < programs.size())
    {
        for (size_t i = 0; i < programs.size(); i++)
            if (!visited[i] && programs[i].arrival <= current_time)
                visited[i] = true;

        ProgramInfo *shortest = nullptr;
        for (size_t i = 0; i < programs.size(); i++)
        {
            if (visited[i] && remaining[programs[i].pid] > 0)
            {
                if (shortest == nullptr ||
                    remaining[programs[i].pid] < remaining[shortest->pid])
                    shortest = &programs[i];
            }
        }

        if (shortest == nullptr)
        {
            for (size_t i = 0; i < programs.size(); i++)
            {
                if (!visited[i])
                {
                    execution_blocks.push_back({0, programs[i].arrival - current_time});
                    current_time = programs[i].arrival;
                    break;
                }
            }
            continue;
        }

        remaining[shortest->pid]--;
        current_time++;

        if (!execution_blocks.empty() && execution_blocks.back().pid == shortest->pid)
            execution_blocks.back().time_frame++;
        else
            execution_blocks.push_back({shortest->pid, 1});

        if (remaining[shortest->pid] == 0)
            completed_program_count++;
    }

    return execution_blocks;
}