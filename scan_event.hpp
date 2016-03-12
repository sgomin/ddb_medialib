#ifndef SCAN_EVENT_HPP
#define	SCAN_EVENT_HPP

#include "database.hpp"
#include "event_queue.hpp"

struct ScanEvent
{
	enum Type { ADDED, DELETED, UPDATED };
	
	Type		type;
	RecordID	id;
};

typedef SimpleEventQueue<ScanEvent> ScanEventQueue;

#endif	/* SCAN_EVENT_HPP */

