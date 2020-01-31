#include <iostream>
#include <asio.hpp>

int main(int argc, char* argv[])
{
    try
    {
        //if (argc != 2)
        //{
        //    std::cerr << "Usage: client <host>" << std::endl;
        //    return 1;
        //}

        asio::io_context io_context;

        asio::ip::udp::resolver resolver(io_context);
        asio::ip::udp::endpoint receiver_endpoint =
            *resolver.resolve(asio::ip::udp::v4(), "127.0.0.1", "7777").begin();

        asio::ip::udp::socket socket(io_context);
        socket.open(asio::ip::udp::v4());

        std::array<char, 1> send_buf = { { 0 } };
        socket.send_to(asio::buffer(send_buf), receiver_endpoint);

        std::cout << "sent" << std::endl;

        for (;;) {
            std::array<char, 65535> recv_buf;
            asio::ip::udp::endpoint sender_endpoint;
            std::cout << "before receive" << std::endl;
            size_t len = socket.receive_from(
                asio::buffer(recv_buf), sender_endpoint);
            //std::cout.write(recv_buf.data(), len);
            std::cout << "len: " << len << std::endl;
        }

    } catch (std::exception & e)
    {
        std::cerr << e.what() << std::endl;
    }

    std::cout << "done" << std::endl;
    getchar();

    return 0;
}