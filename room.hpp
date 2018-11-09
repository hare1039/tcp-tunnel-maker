#ifndef ROOM_HPP_
#define ROOM_HPP_

#pragma once

#include <set>
#include <deque>
#include <string_view>
#include "basic.hpp"

namespace pika
{

class room : public std::enable_shared_from_this<room>
{
    lib::tcp::socket socket_;
    lib::tcp::socket target_server_socket_;
    boost::asio::io_context::strand strand_;

public:
    room(lib::tcp::socket && client, lib::tcp::socket && target_server, boost::asio::io_context& io):
        socket_{std::move(client)},
        target_server_socket_{std::move(target_server)},
        strand_{io} {}

    lib::awaitable<void> start(std::deque<std::shared_ptr<room>> & rooms_)
    {
        try
        {
            auto self     = shared_from_this();
            auto token    = co_await lib::this_coro::token();
            auto executor = co_await lib::this_coro::executor();

            BOOST_SCOPE_EXIT (self, &rooms_) {
                auto it = std::remove(rooms_.begin(), rooms_.end(), self->shared_from_this());
                rooms_.erase(it, rooms_.end());
                std::cout << "closing room" << std::endl;
            } BOOST_SCOPE_EXIT_END;

            lib::co_spawn(executor,
                          [self]() mutable
                          {
                              return self->redir(self->socket_, self->target_server_socket_);
                          },
                          lib::detached);
            auto && stoc = self->redir(self->target_server_socket_, self->socket_);

            std::cout << "setup room" << std::endl;
            co_await stoc;
        }
        catch (const std::exception & e)
        {
            std::cerr << "room::start() exception: " << e.what() << std::endl;
        }
        co_return;
    }

    lib::awaitable<void> redir(lib::tcp::socket &from, lib::tcp::socket &to)
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();
        auto self     = shared_from_this();
        try
        {
            std::array<char, def::bufsize> raw_buf;
            for (;;)
            {
                std::size_t read_n = co_await from.async_read_some(boost::asio::buffer(raw_buf), token);
                lib::co_spawn(executor,
                              boost::asio::bind_executor(self->strand_,
                                  [self, raw_buf, read_n, &to]() mutable
                                  {
                                      return self->send(to, raw_buf, read_n);
                                  }),
                              lib::detached);
            }
        }
        catch (const std::exception & e)
        {
            std::cerr << "room::redir() exception: " << e.what() << std::endl;
        }
        boost::system::error_code ec;
        from.shutdown(lib::tcp::socket::shutdown_receive, ec);
        if (ec)
            std::cerr << ec.message() << std::endl;

        to.shutdown(lib::tcp::socket::shutdown_send, ec);
        if (ec)
            std::cerr << ec.message() << std::endl;

        co_return;
    }

private:
    lib::awaitable<void> send(lib::tcp::socket &to, std::array<char, def::bufsize>& raw_buf, std::size_t& read_n)
    {
        auto self   = shared_from_this();
        auto token  = co_await lib::this_coro::token();
        std::ignore = co_await boost::asio::async_write(to, boost::asio::buffer(raw_buf, read_n), token);
        co_return;
    }
};

}// namespace pika

#endif // ROOM_HPP_
