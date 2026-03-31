#ifndef ALGO_H
#define ALGO_H

#include <vector>
#include "structs.hpp"
#include "types.hpp"

// TODO: make Algo class into 3 functions, run (public) sets up everything, execurte (executes the algo) and demolish (finishes domolishing)
class Algo
{
public:
    virtual std::vector<ExecutionBlock> run(std::vector<ProgramInfo>) = 0;

    size_t current_time = 0;
};

class FirstComeFirstServer : public Algo
{
public:
    std::vector<ExecutionBlock> run(std::vector<ProgramInfo> programs) override;
};

/// @brief The Non premptive version of algo
class ShortestJobFirst : public Algo
{
public:
    std::vector<ExecutionBlock> run(std::vector<ProgramInfo> programs) override;

private:
    void pushToQueue(std::vector<bool> &visited, std::vector<ProgramInfo> &programs, scheduler::burst_priority_queue &ready_queue);
};

/// @brief The Non premptive version of algo
// TODO: implement
class ShortestRemainingTimeFirst : public Algo
{
public:
    std::vector<ExecutionBlock> run(std::vector<ProgramInfo> programs) override;
};

class RoundRobin : public Algo
{
public:
    std::vector<ExecutionBlock> run(std::vector<ProgramInfo> programs) override;
};

class Priority : public Algo
{
    std::vector<ExecutionBlock> run(std::vector<ProgramInfo> programs) override;
};

#endif