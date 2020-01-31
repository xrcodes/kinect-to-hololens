#include <iostream>
#include <asio.hpp>

int main(int argc, char* argv[])
{
    try
    {
        asio::io_context io_context;

        asio::ip::udp::resolver resolver(io_context);
        asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(asio::ip::udp::v4(), "localhost", "daytime").begin();

        asio::ip::udp::socket socket(io_context);
        socket.open(asio::ip::udp::v4());

        std::array<char, 1> send_buf = { { 0 } };
        socket.send_to(asio::buffer(send_buf), receiver_endpoint);

        std::array<char, 128> recv_buf;
        asio::ip::udp::endpoint sender_endpoint;
        size_t len = socket.receive_from(asio::buffer(recv_buf), sender_endpoint);

        std::cout.write(recv_buf.data(), len);
    } catch (std::exception & e)
    {
        std::cerr << e.what() << std::endl;
    }

    getchar();

    return 0;
}