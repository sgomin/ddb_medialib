#ifndef SCAN_THREAD_HPP
#define	SCAN_THREAD_HPP

#include "settings.hpp"
#include "database.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
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
            const Records& oldRecords,
            bool recursive);
    
    
    const Settings::Directories dirs_;
    Database&                   db_;
    bool                        changed_;
};

#endif	/* SCAN_THREAD_HPP */

