#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

#include <assert.h>

template<typename Event>
class SimpleEventQueue
{
public:
	
	template<typename Event2>
	void push(Event2&& event)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.push_back(std::forward<Event2>(event));
	}
	
	bool empty() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.empty();
	}
	
	Event pop()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		
		assert(!queue_.empty());
		
		if (queue_.empty())
		{
			throw std::runtime_error("can't pop empty queue");
		}
		
		Event event = std::move(queue_.front());
		queue_.pop_front();
		return event;
	}
	
private:
	mutable std::mutex	mutex_;
	std::deque<Event>	queue_;
};


template<typename Event>
class EventQueue
{
public:
	
	template<typename Event2>
	void push(Event2&& event)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		queue_.push_back(std::forward<Event2>(event));
		cond_.notify_one();
	}
	
	Event pop()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		
		cond_.wait(lock, [this](){ return !queue_.empty(); });

		assert(!queue_.empty());
		Event event = std::move(queue_.front());
		queue_.pop_front();
		return event;
	}
	
private:
	std::mutex				mutex_;
	std::condition_variable	cond_;
	std::deque<Event>		queue_;
};

