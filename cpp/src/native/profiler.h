#pragma once

#include <unordered_map>
#include "native/kh_native.h"

namespace kh
{
class Profiler
{
public:
    Profiler()
        : time_point_{tt::TimePoint::now()}, numbers_ {}
    {
    }
    void reset()
    {
        time_point_ = tt::TimePoint::now();
        numbers_.clear();
    }
    void setNumber(std::string name, float number)
    {
        auto find_result{numbers_.find(name)};
        if (find_result == numbers_.end()) {
            numbers_.insert({name, number});
        } else {
            find_result->second = number;
        }
    }
    void addNumber(std::string name, float number)
    {
        auto find_result{numbers_.find(name)};
        if (find_result == numbers_.end()) {
            numbers_.insert({name, number});
        } else {
            find_result->second += number;
        }
    }
    tt::TimeDuration getElapsedTime()
    {
        return time_point_.elapsed_time();
    }
    float getNumber(std::string name)
    {
        auto find_result{numbers_.find(name)};
        if (find_result == numbers_.end()) {
            return 0.0f;
        } else {
            return find_result->second;
        }
    }

private:
    tt::TimePoint time_point_;
    std::unordered_map<std::string, float> numbers_;
};
}