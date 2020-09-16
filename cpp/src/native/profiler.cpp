#include "profiler.h"

namespace kh
{
Profiler::Profiler()
    : time_point_{tt::TimePoint::now()}, numbers_{}
{
}

void Profiler::reset()
{
    time_point_ = tt::TimePoint::now();
    numbers_.clear();
}

void Profiler::setNumber(std::string name, float number)
{
    auto find_result{numbers_.find(name)};
    if (find_result == numbers_.end()) {
        numbers_.insert({name, number});
    } else {
        find_result->second = number;
    }
}

void Profiler::addNumber(std::string name, float number)
{
    auto find_result{numbers_.find(name)};
    if (find_result == numbers_.end()) {
        numbers_.insert({name, number});
    } else {
        find_result->second += number;
    }
}

tt::TimeDuration Profiler::getElapsedTime()
{
    return time_point_.elapsed_time();
}

float Profiler::getNumber(std::string name)
{
    auto find_result{numbers_.find(name)};
    if (find_result == numbers_.end()) {
        return 0.0f;
    } else {
        return find_result->second;
    }
}
}
