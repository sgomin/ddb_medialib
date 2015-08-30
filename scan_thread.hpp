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
    void scanDir(const fs::path& path, const RecordID& parentID, bool recursive);
    void scanFile(const fs::path& path, const RecordID& parentID);
    
    RecordID addDir(const fs::path& path, const RecordID& parentID);
    RecordID addFile(const fs::path& path, const RecordID& parentID, time_t lastWriteTime);
    void updateFile(const RecordID& id, Record& record);
    
    const Settings::Directories dirs_;
    Database&                   db_;
    bool                        changed_;
};

#endif	/* SCAN_THREAD_HPP */

