#pragma once

#include "native/kh_native.h"

namespace kh
{
struct VideoRendererState
{
    int frame_id{-1};
    tt::TimePoint last_frame_time_point{tt::TimePoint::now()};
};
}