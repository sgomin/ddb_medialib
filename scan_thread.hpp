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

#include <glibmm/dispatcher.h>

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
    ScanThread(const SettingsProvider& settings,
			   const Extensions& extensions,
               DbOwnerPtr&& db,
			   ScanEventSink eventSink,
               Glib::Dispatcher& onChangedDisp);
    ~ScanThread();
    
	void restart();
	
    void operator() ();

private:
    struct Changes
    {
        bool empty() const;
        
        void addEntry(FileInfo&& data);
        void delEntry(const RecordID& id);
        void replaceEntry(FileRecord&& record);
    
        Changes& operator+= (Changes&& other);
        
        std::list<RecordID>   deleted;
        std::list<FileRecord> changed;
        std::list<FileInfo>   added;
    };
    
    template<typename EntriesIt>
    Changes scanDir(
            const RecordID& dirId, 
            EntriesIt itEntriesBegin, 
            EntriesIt itEntriesEnd);
    
    Changes scanEntry(
            const fs::path& path, 
            const RecordID& parentID, 
            FileRecords& oldRecords,
            bool recursive);
    
    Changes checkDir(const FileRecord& recDir);
    
    bool shouldBreak() const;
	bool isSupportedExtension(const fs::path& fileName);
    
    Changes scanDirs();
    bool save(Changes&& changes);
    
	std::thread					thread_;
	std::condition_variable_any	cond_;
	std::atomic<bool>			stop_;
	std::atomic<bool>			restart_;
    const SettingsProvider&		settings_;
    const Extensions			extensions_;
    DbOwnerPtr                  db_;
	ScanEventSink				eventSink_;
    Glib::Dispatcher&           onChangedDisp_;
	std::chrono::milliseconds	sleepTime_ = std::chrono::seconds(5);
};

#endif	/* SCAN_THREAD_HPP */

