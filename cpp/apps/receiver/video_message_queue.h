#pragma once

#include <vector>
#include <memory>
#include "native/kh_native.h"

namespace kh
{
struct VideoMessageQueue
{
    std::vector<std::pair<int, std::shared_ptr<VideoSenderMessage>>> messages;
};
}