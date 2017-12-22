#pragma once

#include <boost/thread/sync_queue.hpp>

#if BOOST_VERSION >= 105600 // 1_56_0
#   include <boost/thread/concurrent_queues/queue_views.hpp>
#else

namespace boost
{
namespace concurrent
{
  template <typename Queue>
  class queue_back_view
  {
   Queue* queue;
  public:
    typedef typename Queue::value_type value_type;
    typedef typename Queue::size_type size_type;

    // Constructors/Assignment/Destructors
    queue_back_view(Queue& q) BOOST_NOEXCEPT : queue(&q) {}

    // Observers
    bool empty() const  { return queue->empty(); }
    bool full() const { return queue->full(); }
    size_type size() const { return queue->size(); }
    bool closed() const { return queue->closed(); }

    // Modifiers
    void close() { queue->close(); }

    void push(const value_type& x) { queue->push(x); }

    queue_op_status try_push(const value_type& x) { return queue->try_push(x); }

    queue_op_status nonblocking_push(const value_type& x) { return queue->nonblocking_push(x); }
    queue_op_status wait_push(const value_type& x) { return queue->wait_push(x); }

    void push(BOOST_THREAD_RV_REF(value_type) x) { queue->push(boost::move(x)); }
    queue_op_status try_push(BOOST_THREAD_RV_REF(value_type) x) { return queue->try_push(boost::move(x)); }
    queue_op_status nonblocking_push(BOOST_THREAD_RV_REF(value_type) x) { return queue->nonblocking_push(boost::move(x)); }
    queue_op_status wait_push(BOOST_THREAD_RV_REF(value_type) x) { return queue->wait_push(boost::move(x)); }
  };

  template <typename Queue>
  class queue_front_view
  {
   Queue* queue;
  public:
    typedef typename Queue::value_type value_type;
    typedef typename Queue::size_type size_type;

    // Constructors/Assignment/Destructors
    queue_front_view(Queue& q) BOOST_NOEXCEPT : queue(&q) {}

    // Observers
    bool empty() const  { return queue->empty(); }
    bool full() const { return queue->full(); }
    size_type size() const { return queue->size(); }
    bool closed() const { return queue->closed(); }

    // Modifiers
    void close() { queue->close(); }

    void pull(value_type& x) { queue->pull(x); };
    // enable_if is_nothrow_copy_movable<value_type>
    value_type pull()  { return queue->pull(); }

    queue_op_status try_pull(value_type& x) { return queue->try_pull(x); }

    queue_op_status nonblocking_pull(value_type& x) { return queue->nonblocking_pull(x); }

    queue_op_status wait_pull(value_type& x) { return queue->wait_pull(x); }

  };
} // namespace concurrent

using concurrent::queue_back_view;
using concurrent::queue_front_view;

} // namespace boost

#endif
