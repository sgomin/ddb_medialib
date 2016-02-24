#ifndef SCAN_THREAD_HPP
#define	SCAN_THREAD_HPP

#include "settings.hpp"
#include "database.hpp"
#include "scan_event.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/algorithm/string/predicate.hpp>

#include <set>
#include <string>
#include <atomic>
#include <thread>
#include <condition_variable>

#include <db_cxx.h>

struct CaseCompare
{
	bool operator() (const std::string& left, const std::string& right) const
	{
		return boost::ilexicographical_compare(left, right);
	}
};

typedef std::set<std::string, CaseCompare> Extensions;

class ScanThread
{
public:
    ScanThread(const Settings::Directories& dirs,
			   const Extensions& extensions,
               Database& db,
			   ScanEventQueue& eventSink);
    ~ScanThread();
    
    void operator() ();

private:
    template<typename EntriesIt>
    void scanDir(
            const RecordID& dirId, 
            EntriesIt itEntriesBegin, 
            EntriesIt itEntriesEnd,
            bool recursive);
    
    void scanEntry(
            const fs::path& path, 
            const RecordID& parentID, 
            Records& oldRecords,
            bool recursive);
    
    
	std::thread					thread_;
	std::condition_variable_any	cond_;
	std::atomic<bool>			stop_;
    const Settings::Directories dirs_;
    const Extensions			extensions_;
    Database&                   db_;
	ScanEventQueue&				eventSink_;
	bool                        changed_ = false;
	std::chrono::milliseconds	sleepTime_ = std::chrono::milliseconds(100);
};

#endif	/* SCAN_THREAD_HPP */

