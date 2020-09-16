#pragma once

#include <map>
#include "core/tt_core.h"

namespace kh
{
class Profiler
{
public:
    Profiler();
    void reset();
    void setNumber(std::string name, float number);
    void addNumber(std::string name, float number);
    tt::TimeDuration getElapsedTime();
    float getNumber(std::string name);

private:
    tt::TimePoint time_point_;
    std::map<std::string, float> numbers_;
};
}