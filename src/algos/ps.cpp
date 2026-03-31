#include <vector>
#include <structs.hpp>
#include <algo.hpp>
#include <algorithm>

std::vector<ExecutionBlock> Priority::run(std::vector<ProgramInfo> programs)
{
    current_time = 0;
    scheduler::priority_queue ready_queue(program_priority_compare);
    std::vector<bool> visited(programs.size(), false);
    std::vector<ExecutionBlock> execution_order;

    std::sort(programs.begin(), programs.end(), [](ProgramInfo &a, ProgramInfo &b)
              { return a.arrival < b.arrival; });

    size_t completed_program_count = 0;
    current_time = 0;

    while (completed_program_count < programs.size())
    {
        for (size_t i = 0; i < programs.size(); i++)
            if (!visited[i] && programs[i].arrival <= current_time)
            {
                ready_queue.push(programs[i]);
                visited[i] = true;
            }

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
            continue;
        }

        ProgramInfo current_program = ready_queue.top();

        for (size_t i = 0; i < programs.size(); i++)
            if (!visited[i] && programs[i].arrival <= current_time + current_program.burst)
            {
                ready_queue.push(programs[i]);
                visited[i] = true;
            }

        current_program = ready_queue.top();
        ready_queue.pop();

        current_time += current_program.burst;
        completed_program_count++;
        execution_order.push_back({current_program.pid, current_program.burst});
    }

    return execution_order;
}