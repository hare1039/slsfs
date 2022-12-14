#pragma once

#ifndef SOCKET_WRITER_HPP__
#define SOCKET_WRITER_HPP__

#include <boost/asio.hpp>

#include <oneapi/tbb/concurrent_queue.h>

namespace slsfs::socket_writer
{

using boost_callback = std::function<void(boost::system::error_code, std::size_t)>;

template<typename PacketPointer, typename BufType>
struct write_job
{
    PacketPointer pack;
    std::shared_ptr<BufType> bufptr;
    std::shared_ptr<boost_callback> next;
};

namespace v1
{

template<typename PacketPointer, typename BufType>
class socket_writer
{
    boost::asio::io_context & io_context_;
    boost::asio::io_context::strand write_io_strand_;
    oneapi::tbb::concurrent_queue<write_job<PacketPointer, BufType>> write_queue_;
    boost::asio::ip::tcp::socket& socket_;
    std::atomic<bool> is_writing_ = false;

    void start_write_one_packet()
    {
        write_job<PacketPointer, BufType> job;
        if (not write_queue_.try_pop(job))
            is_writing_.store(false);
        else
        {
            if (job.bufptr == nullptr)
                job.bufptr = job.pack->serialize();

            boost::asio::async_write(
                socket_,
                boost::asio::buffer(job.bufptr->data(), job.bufptr->size()),
                boost::asio::bind_executor(
                    write_io_strand_,
                    [this, job] (boost::system::error_code ec, std::size_t transferred_size) {
                        std::invoke(*job.next, ec, transferred_size);
                        if (ec)
                        {
                            is_writing_.store(false);
                            BOOST_LOG_TRIVIAL(error) << "socket writer get error: " << ec.message();
                            return;
                        }

                        start_write_one_packet();
                    }));
        }
    }

public:
    socket_writer(boost::asio::io_context &io, boost::asio::ip::tcp::socket &s):
        io_context_{io}, write_io_strand_{io}, socket_{s} {}

    void start_write_socket(PacketPointer pack,
                            std::shared_ptr<boost_callback> next,
                            std::shared_ptr<BufType> bufptr = nullptr)
    {
        write_queue_.push(write_job(pack, bufptr, next));
        if (not is_writing_)
        {
            is_writing_.store(true);
            start_write_one_packet();
        }
    }
};

} // namespace v1

namespace v2
{

template<typename PacketPointer, typename BufType>
class socket_writer
{
    boost::asio::io_context & io_context_;
    boost::asio::io_context::strand write_io_strand_;
    oneapi::tbb::concurrent_queue<write_job<PacketPointer, BufType>> write_queue_;
    boost::asio::ip::tcp::socket& socket_;

    void start_write_one_packet()
    {
        write_job<PacketPointer, BufType> job;

        if (write_queue_.try_pop(job))
        {
            if (job.bufptr == nullptr)
                job.bufptr = job.pack->serialize();

            boost::asio::async_write(
                socket_,
                boost::asio::buffer(job.bufptr->data(), job.bufptr->size()),
                boost::asio::bind_executor(
                    write_io_strand_,
                    [this, job] (boost::system::error_code ec, std::size_t transferred_size) {
                        std::invoke(*job.next, ec, transferred_size);
                        if (ec)
                        {
                            BOOST_LOG_TRIVIAL(error) << "socket writer get error: " << ec.message();
                            return;
                        }

                        start_write_one_packet();
                    }));
        }
    }

public:
    socket_writer(boost::asio::io_context &io, boost::asio::ip::tcp::socket &s):
        io_context_{io}, write_io_strand_{io}, socket_{s} {}

    void start_write_socket(PacketPointer pack,
                            std::shared_ptr<boost_callback> next,
                            std::shared_ptr<BufType> bufptr = nullptr)
    {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push(write_job(pack, bufptr, next));
        if (not write_in_progress)
            start_write_one_packet();
    }
};

} // namespace v2

template<typename PacketPointer, typename BufType>
using socket_writer = v1::socket_writer<PacketPointer, BufType>;

} // namespace slsfs


#endif // SOCKET_WRITER_HPP__
