#include "algo.hpp"
#include <queue>
#include <algorithm>
#include <iostream>
#include <types.hpp>

std::vector<ExecutionBlock> ShortestJobFirst::run(std::vector<ProgramInfo> programs)
{
    current_time = 0;
    scheduler::burst_priority_queue ready_queue(program_burst_compare);

    std::vector<bool> visited(programs.size(), false);

    std::vector<ExecutionBlock> execution_order;

    std::sort(programs.begin(), programs.end(), [](ProgramInfo &a, ProgramInfo &b)
              { return a.arrival < b.arrival; });

    size_t completed_program_count = 0;

    while (completed_program_count < programs.size()) // run till all programs are done
    {
        pushToQueue(visited, programs, ready_queue);

        if (ready_queue.empty())
        {
            for (size_t i = 0; i < programs.size(); i++)
            {
                if (!visited[i])
                {
                    execution_order.push_back({0, programs[i].arrival - current_time});
                    current_time = programs[i].arrival;
                    break;
                }
            }
            for (size_t i = 0; i < programs.size(); i++)
            {

                if (!visited[i] && programs[i].arrival <= current_time)
                {
                    ready_queue.push(programs[i]);
                    visited[i] = true;
                }
            }
        }

        ProgramInfo current_program = ready_queue.top();
        ready_queue.pop();
        completed_program_count++;
        current_time += current_program.burst;
        execution_order.push_back({current_program.pid, current_program.burst});
    }

    return execution_order;
}

void ShortestJobFirst::pushToQueue(std::vector<bool> &visited, std::vector<ProgramInfo> &programs, scheduler::burst_priority_queue &ready_queue)
{
    for (size_t i = 0; i < programs.size(); i++)
    {

        if (!visited[i] && programs[i].arrival <= current_time)
        {
            ready_queue.push(programs[i]);
            visited[i] = true;
        }
    }
}