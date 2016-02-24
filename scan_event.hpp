#ifndef SCAN_EVENT_HPP
#define	SCAN_EVENT_HPP

#include "database.hpp"
#include "event_queue.hpp"

struct ScanEvent
{
	enum Type { ADDED, DELETED };
	
	Type		type;
	RecordID	id;
};

typedef EventQueue<ScanEvent> ScanEventQueue;

#endif	/* SCAN_EVENT_HPP */

