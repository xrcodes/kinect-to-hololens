#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace kh
{
//const int KH_PACKET_SIZE = 1500;
const int KH_PACKET_SIZE = 1472;
const int KH_PACKET_HEADER_SIZE = 17;
const int KH_MAX_PACKET_CONTENT_SIZE = KH_PACKET_SIZE - KH_PACKET_HEADER_SIZE;

template<class T>
void copy_to_packet(const T& t, std::vector<uint8_t>& packet, int& cursor)
{
    memcpy(packet.data() + cursor, &t, sizeof(T));
    cursor += sizeof(T);
}

template<class T>
T copy_from_packet(const std::vector<uint8_t>& packet, int& cursor)
{
    T t;
    memcpy(&t, packet.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return t;
}
}