#ifndef SCAN_THREAD_HPP
#define	SCAN_THREAD_HPP

#include "settings.hpp"
#include "database.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <atomic>
#include <thread>
#include <condition_variable>

#include <db_cxx.h>

class ScanThread
{
public:
    ScanThread(const Settings::Directories& dirs,
               Database& db);
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
    Database&                   db_;
    bool                        changed_ = false;
	std::chrono::milliseconds	sleepTime_ = std::chrono::milliseconds(100);
};

#endif	/* SCAN_THREAD_HPP */

