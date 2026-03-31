#include "algo.hpp"
#include <iostream>

std::vector<ProgramInfo> test1 = {
    {1, 3, 5, 0},
    {2, 5, 2, 0},
    {3, 8, 1, 0},
};

std::vector<ProgramInfo> test2 = {
    {1, 0, 5, 0},
    {2, 0, 2, 0},
    {3, 0, 8, 0},
};

std::vector<ProgramInfo> test3 = {
    {1, 0, 2, 0},
    {2, 0, 4, 0},
    {3, 10, 1, 0},
};

std::vector<ProgramInfo> test4 = {
    {1, 0, 8, 0},
    {2, 1, 2, 0},
    {3, 2, 1, 0},
    {4, 3, 4, 0},
};

std::vector<ProgramInfo> test5 = {
    {1, 0, 5, 3},
    {2, 0, 2, 1},
    {3, 0, 8, 2},
    {4, 2, 3, 4},
};

auto print = [](const std::string &name, std::vector<ExecutionBlock> blocks)
{
    std::cout << "=== " << name << " ===\n";
    std::cout << "PID\tTime\n";
    std::cout << "---\t----\n";
    size_t time = 0;
    for (const ExecutionBlock &block : blocks)
    {
        if (block.pid == 0)
            std::cout << "IDLE\t" << block.time_frame << " (t=" << time << " to t=" << time + block.time_frame << ")\n";
        else
            std::cout << block.pid << "\t" << block.time_frame << " (t=" << time << " to t=" << time + block.time_frame << ")\n";
        time += block.time_frame;
    }
    std::cout << "\n";
};

void run_all(Algo &algo, const std::string &algo_name)
{
    std::cout << "\n========== " << algo_name << " ==========\n";
    print("Test 1 - Idle at start",            algo.run(test1));
    print("Test 2 - All arrive at t=0",         algo.run(test2));
    print("Test 3 - Idle in middle",            algo.run(test3));
    print("Test 4 - Shortest not first",        algo.run(test4));
    print("Test 5 - Priority",                  algo.run(test5));
}

int main()
{
    FirstComeFirstServer    fcfs;
    ShortestJobFirst        sjf;
    ShortestRemainingTimeFirst srtf;
    RoundRobin              rr;
    Priority                priority;

    run_all(fcfs,     "First Come First Serve");
    run_all(sjf,      "Shortest Job First");
    run_all(srtf,     "Shortest Remaining Time First");
    run_all(rr,       "Round Robin");
    run_all(priority, "Priority");

    return 0;
}