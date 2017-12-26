#ifndef SCAN_EVENT_HPP
#define	SCAN_EVENT_HPP

#include "database.hpp"

#include <boost/thread/sync_queue.hpp>
#include <boost/thread/synchronized_value.hpp>
#include "queue_views.hpp"

struct ScanEvent
{
	enum Type { ADDED, DELETED, UPDATED };
	
	Type		type;
	RecordID	id;
};

typedef boost::sync_queue<ScanEvent> ScanEventQueue;
typedef boost::queue_back_view<ScanEventQueue> ScanEventSink;
typedef boost::queue_front_view<ScanEventQueue> ScanEventSource;

struct ActiveRecords
{
    RecordIDs             ids;
    std::function<void()> onChanged;
};

using ActiveRecordsSync = boost::synchronized_value<ActiveRecords>;


#endif	/* SCAN_EVENT_HPP */

