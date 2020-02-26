#pragma once

#include <chrono>

namespace kh
{
class TimeDuration
{
public:
    TimeDuration()
        : duration_{}
    {
    }

    TimeDuration(std::chrono::duration<float, std::milli> duration)
        : duration_{duration}
    {
    }
    
    float sec() const
    {
        return duration_.count() / 1000.0f;
    }

    float ms() const
    {
        return duration_.count();
    }

private:
    std::chrono::duration<float, std::milli> duration_;
};

class TimePoint
{
public:
    TimePoint()
        : time_point_{}
    {
    }

    TimePoint(std::chrono::time_point<std::chrono::steady_clock> time_point)
        : time_point_{time_point}
    {
    }

    static TimePoint now()
    {
        return TimePoint(std::chrono::steady_clock::now());
    }

    TimeDuration operator-(const TimePoint& other)
    {
        return TimeDuration{time_point_ - other.time_point_};
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> time_point_;
};
}