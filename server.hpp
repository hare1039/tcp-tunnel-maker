#ifndef SERVER_HPP_
#define SERVER_HPP_

#pragma once
#include <thread>
#include <iostream>
#include <cctype>
#include <unordered_map>
#include <optional>
#include "room.hpp"

namespace pika
{

class server
{
    std::deque<std::shared_ptr<room>> rooms_;
    unsigned short listen_port_ = 7000;
    lib::tcp::resolver::results_type target_server_ep_;
public:
    server(unsigned short port):
        listen_port_{port} {}

    lib::awaitable<void> run(std::string_view path)
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        lib::tcp::acceptor acceptor(executor.context(), {lib::tcp::v4(), listen_port_});
        for (;;)
        {
            lib::tcp::socket socket = co_await acceptor.async_accept(token);
            lib::co_spawn(executor,
                          [socket = std::move(socket), path, this]() mutable
                          {
                              return this->dispatch_socket(std::move(socket), path);
                          },
                          lib::detached);
        }
    }

    lib::awaitable<void> dispatch_socket(lib::tcp::socket && socket,
                                         std::string_view start_client)
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        try
        {
            std::string line;

            // start by innermost client
            if (start_client != "")
                line = start_client;
            else
            {
                boost::asio::streambuf readbuf;
                std::ignore = co_await boost::asio::async_read_until(socket, readbuf, '\n', token);
                line = std::string((std::istreambuf_iterator<char>(&readbuf)), std::istreambuf_iterator<char>());
            }
            std::string_view next = parse(line, executor.context());

            lib::tcp::socket target_server_socket{executor.context()};
            std::ignore = co_await boost::asio::async_connect(target_server_socket, target_server_ep_, token);

            if (next != "\n")
                std::ignore = co_await boost::asio::async_write(target_server_socket, boost::asio::buffer(next.data(), next.size()), token);

            lib::co_spawn(executor,
                          [socket    = std::move(socket),
                           ts_socket = std::move(target_server_socket),
                           this]() mutable
                          {
                              boost::asio::io_context &io = socket.get_executor().context();
                              auto r = std::make_shared<room>(std::move(socket),
                                                              std::move(ts_socket),
                                                              io);
                              rooms_.push_back(r);
                              return r->start(rooms_);
                          },
                          lib::detached);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception: " <<  e.what() << std::endl;
        }
    }

    std::string_view parse(std::string_view sv, boost::asio::io_context &io_context)
    {
        std::string_view::iterator delim = std::find_if(sv.begin(), sv.end(), [] (char c) { return std::isspace(c); });
        std::string_view server_sv = sv.substr(0, std::distance(sv.begin(), delim));
        auto it = std::find(server_sv.begin(), server_sv.end(), ':');
        std::string_view server_host = sv.substr(0, std::distance(server_sv.begin(), it));
        std::string_view server_port = sv.substr(std::distance(server_sv.begin(), it) + 1, std::distance(it, server_sv.end()) - 1);

        lib::tcp::resolver resolver{io_context};
        target_server_ep_ = resolver.resolve(server_host, server_port);
        std::string_view next = sv.substr(std::distance(sv.begin(), delim));

        next.remove_prefix(std::min(next.find_first_not_of(" "), next.size()));

        return next;
    }
};

}// namespace pika


#endif // SERVER_HPP_
