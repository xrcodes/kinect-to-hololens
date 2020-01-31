#include <iostream>
#include <asio.hpp>

std::string make_daytime_string()
{
    time_t now = time(0);
    return ctime(&now);
}

int main(int argc, char* argv[])
{
    try
    {
        asio::io_context io_context;

        asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), 13));

        for (;;)
        {
            std::array<char, 1> recv_buf;
            asio::ip::udp::endpoint remote_endpoint;
            std::error_code error;
            socket.receive_from(asio::buffer(recv_buf),
                                remote_endpoint, 0, error);

            if (error && error != asio::error::message_size)
                throw std::system_error(error);

            std::string message = make_daytime_string();

            std::error_code ignored_error;
            socket.send_to(asio::buffer(message),
                           remote_endpoint, 0, ignored_error);
        }
    }
    catch (std::exception & e)
    {
        std::cerr << e.what() << std::endl;
    }

    getchar();

    return 0;
}