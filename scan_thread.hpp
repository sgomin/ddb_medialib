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
               DbOwner & db,
               ScanEventSink eventSink,
               Glib::Dispatcher& onChangedDisp,
               ActiveRecordsSync& activeFiles);
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
    
    template<typename EntriesRange>
    Changes scanDir(
            const RecordID& dirId, 
            EntriesRange const& entries);
    
    Changes scanEntry(
            const fs::path& path, 
            const RecordID& parentID, 
            FileRecords& oldRecords,
            bool recursive);
    
    Changes checkDir(const FileRecord& recDir);
    
    bool shouldBreak() const;
    bool isSupportedExtension(const fs::path& fileName);
    
    Changes scanDirs(bool isIdle);
    bool save(Changes&& changes);
    
    void onActiveFilesChanged(bool restart);
    
    std::thread                 thread_;
    std::condition_variable_any	cond_;
    std::atomic<bool>           stop_;
    std::atomic<bool>           restart_;
    std::atomic<bool>           continue_;
    const SettingsProvider&     settings_;
    const Extensions            extensions_;
    DbOwner&                    db_;
    ScanEventSink               eventSink_;
    Glib::Dispatcher&           onChangedDisp_;
    ActiveRecordsSync&          activeFiles_;
};

#endif	/* SCAN_THREAD_HPP */

