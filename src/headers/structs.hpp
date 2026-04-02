#ifndef PROGRAMINFO_H
#define PROGRAMINFO_H
#include <cstddef>

typedef struct
{
    size_t pid;
    size_t arrival;
    size_t burst;
    size_t priority;
} ProgramInfo;

inline auto program_burst_compare = [](ProgramInfo &a, ProgramInfo &b)
{ return a.burst > b.burst; };

inline auto program_priority_compare = [](ProgramInfo &a, ProgramInfo &b)
{
    return a.priority > b.priority;
};

inline auto program_arrival_compare = [](ProgramInfo &a, ProgramInfo &b)
{
    return a.arrival < b.arrival;
};

typedef struct
{
    size_t pid;
    size_t time_frame;
} ExecutionBlock;

#endif