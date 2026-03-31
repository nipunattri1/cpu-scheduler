#include <vector>
#include <structs.hpp>
#include <algo.hpp>
#include <algorithm>
#include <unordered_map>
std::vector<ExecutionBlock> RoundRobin::run(std::vector<ProgramInfo> programs)
{
    current_time = 0;
    std::sort(programs.begin(), programs.end(), [](ProgramInfo &a, ProgramInfo &b)
              { return a.arrival < b.arrival; });

    constexpr size_t quantum_time = 2;
    size_t completed_program_count = 0;
    std::queue<ProgramInfo> program_queue;
    std::vector<ExecutionBlock> execution_blocks;
    std::unordered_map<size_t, size_t> executed_time;
    std::unordered_map<size_t, bool> in_queue; // has been added to queue
    current_time = 0;

    // push all programs that arrive at t=0
    for (auto &i : programs)
    {
        if (i.arrival <= current_time)
        {
            program_queue.push(i);
            in_queue[i.pid] = true;
        }
    }

    while (completed_program_count < programs.size())
    {
        if (program_queue.empty())
        {
            for (auto &i : programs)
            {
                if (!in_queue[i.pid])
                {
                    execution_blocks.push_back({0, i.arrival - current_time});
                    current_time = i.arrival;
                    program_queue.push(i);
                    in_queue[i.pid] = true;
                    break;
                }
            }
            continue;
        }

        ProgramInfo current = program_queue.front();
        program_queue.pop();

        // initialize executed time if first time seeing this process
        if (executed_time.find(current.pid) == executed_time.end())
            executed_time[current.pid] = 0;

        size_t run_time = std::min(quantum_time, current.burst - executed_time[current.pid]);
        current_time += run_time;
        executed_time[current.pid] += run_time;

        if (execution_blocks.empty() || execution_blocks.back().pid != current.pid)
            execution_blocks.push_back({current.pid, run_time});
        else
            execution_blocks.back().time_frame += run_time;

        for (auto &i : programs)
            if (!in_queue[i.pid] && i.arrival <= current_time)
            {
                program_queue.push(i);
                in_queue[i.pid] = true;
            }

        if (executed_time[current.pid] < current.burst)
            program_queue.push(current);
        else
            completed_program_count++;
    }

    return execution_blocks;
};