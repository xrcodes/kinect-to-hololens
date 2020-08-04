#pragma once

#include "native/kh_native.h"

namespace kh
{
struct VideoRendererState
{
    int frame_id{-1};
    TimePoint last_frame_time_point{TimePoint::now()};
};
}