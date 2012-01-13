/*
 * Copyright (C) 2008 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include "gu_epoll.hpp"
#include "gu_network.hpp"
#include "gu_logger.hpp"
#include "gu_datetime.hpp"
#include "gu_convert.hpp"

#include <stdexcept>

#include <sys/epoll.h>
#include <cerrno>
#include <cstring>

using namespace gu;
using namespace gu::datetime;

/*
 * Mapping between NetworkEvent and EPoll events
 */

static inline int to_epoll_mask(const int mask)
{
    int ret = 0;
    ret |= (mask & gu::net::E_IN ? EPOLLIN : 0);
    ret |= (mask & gu::net::E_OUT ? EPOLLOUT : 0);
    return ret;
}

static inline int to_network_event_mask(const int mask)
{
    int ret = 0;
    if (mask & ~(EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP))
    {
        log_warn << "event mask " << mask << " has unrecognized bits set";
    }
    
    ret |= (mask & EPOLLIN  ? gu::net::E_IN     : 0);
    ret |= (mask & EPOLLOUT ? gu::net::E_OUT    : 0);
    ret |= (mask & EPOLLERR ? gu::net::E_ERROR  : 0);
    ret |= (mask & EPOLLHUP ? gu::net::E_CLOSED : 0);
    return ret;
}

gu::net::EPoll::EPoll() :
    e_fd(-1),
    n_events(0),
    events(16),
    current(events.end())
{
    if ((e_fd = epoll_create(16)) == -1)
    {
        gu_throw_error(errno) << "Could not create epoll";
    }
}

gu::net::EPoll::~EPoll()
{
    int err = closefd(e_fd);

    if (err != 0)
    {
        log_warn << "Error closing epoll socket: " << err;
    }
}
    
void gu::net::EPoll::insert(const PollEvent& epe)
{
    int op = EPOLL_CTL_ADD;

    struct epoll_event ev = {
        to_epoll_mask(epe.get_events()), 
        {epe.get_user_data()}
    };

    int err = epoll_ctl(e_fd, op, epe.get_fd(), &ev);

    if (err != 0)
    {
        gu_throw_error(errno) << "epoll_ctl(" << e_fd << ","<< op <<") failed";
    }

    events.resize(events.size() + 1);
    current = events.end();
    n_events = 0;
}

void gu::net::EPoll::erase(const PollEvent& epe)
{
    int op = EPOLL_CTL_DEL;

    struct epoll_event ev = {0, {0}};

    int err = epoll_ctl(e_fd, op, epe.get_fd(), &ev);

    if (err != 0)
    {
        err = errno;
        log_debug << "epoll erase: " << err << " (" << strerror(err) << ')';
    }

    events.resize(events.size() - 1);
    current = events.end();
    n_events = 0;
}

void gu::net::EPoll::modify(const PollEvent& epe)
{
    int op = EPOLL_CTL_MOD;

    struct epoll_event ev = {
        to_epoll_mask(epe.get_events()), 
        {epe.get_user_data()}
    };

    int err = epoll_ctl(e_fd, op, epe.get_fd(), &ev);
    
    if (err != 0)
    {
        gu_throw_error(errno) << "epoll_ctl(" << op << "," << epe.get_fd()
                              << ") failed";
    }
}

void gu::net::EPoll::poll(const Period& p)
{
    int timeout(p.get_nsecs() < 0 ? -1 : convert(p.get_nsecs()/MSec, int()));
    
    int ret = epoll_wait(e_fd, &events[0], static_cast<int>(events.size()), 
                         timeout);
    if (ret == -1)
    {
        if (errno != EINTR)
        {
            log_warn << "epoll_wait(): " << strerror(errno);
        }
        n_events = 0;
        current = events.end();
    }
    else
    {
        n_events = ret;
        current = events.begin();
    }
}

bool gu::net::EPoll::empty() const
{
    return n_events == 0;
}

gu::net::PollEvent gu::net::EPoll::front() const
    throw (gu::Exception)
{
    if (n_events == 0) gu_throw_fatal << "No events available";

    return PollEvent(-1,
                     to_network_event_mask(current->events),
                     current->data.ptr);
}


void gu::net::EPoll::pop_front()
    throw (gu::Exception)
{
    if (n_events == 0) gu_throw_fatal << "No events available";
    
    --n_events;
    ++current;
}