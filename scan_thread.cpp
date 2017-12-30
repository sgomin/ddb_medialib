#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>
#include <boost/scope_exit.hpp>

#include <thread>

namespace pl = std::placeholders;

ScanThread::ScanThread(
		const SettingsProvider& settings,
		const Extensions& extensions,
		DbOwner& db,
		ScanEventSink eventSink,
        Glib::Dispatcher& onChangedDisp,
        ActiveRecordsSync& activeFiles)
 : stop_(false)
 , restart_(true)
 , continue_(false)
 , settings_(settings)
 , extensions_(extensions)
 , db_(db)
 , eventSink_(eventSink)
 , onChangedDisp_(onChangedDisp)
 , activeFiles_(activeFiles)
{
    thread_ = std::thread(std::ref(*this));
}
    
ScanThread::~ScanThread()
{
	stop_ = true;
	cond_.notify_all();
	thread_.join();
}

void ScanThread::restart()
{
	restart_ = true;
	cond_.notify_all();
}

void ScanThread::onActiveFilesChanged(bool rest)
{
    if (rest)
    {
        restart();
    }
    else
    {
        continue_ = true;
        cond_.notify_all();
    }
}

namespace {
	
const fs::path& getPath(const fs::directory_entry& dirEntry)
{
	return dirEntry.path();
}

fs::path getPath(const Settings::Directories::value_type& dirEntry)
{
	return dirEntry.first;
}
	
bool isRecursive(const fs::directory_entry& dirEntry)
{
	return true;
}

bool isRecursive(const Settings::Directories::value_type& dirEntry)
{
	return dirEntry.second.recursive;
}

struct CmpByPath
{
	bool operator() (const FileRecord& left, const FileRecord& right) const
	{
		return left.second.fileName < right.second.fileName;
	}
};

}


template<typename EntriesRange>
ScanThread::Changes ScanThread::scanDir(
            const RecordID& dirId, 
            EntriesRange const& entries)
{
    Changes result;
    
	if (shouldBreak())
	{
		return result;
	}
    
    std::clog << "[Scan] scanDir #" << dirId << std::endl;
	
    auto oldRecords = db_.childrenFiles(dirId);
    
	std::sort(oldRecords.begin(), oldRecords.end(), CmpByPath());
		
	for (const auto& entry : entries)
	{
		result += scanEntry(getPath(entry), dirId, oldRecords, isRecursive(entry));
		
		if (shouldBreak())
		{
			return result;
		}
	}
	
	for (const FileRecord& missing : oldRecords)
	{
		result.delEntry(missing.first);
	}
    
    return result;
}

ScanThread::Changes ScanThread::scanEntry(
			const fs::path& path, 
			const RecordID& parentID, 
			FileRecords& oldRecords,
			bool recursive)
try
{
    Changes result;
    
	if (shouldBreak())
	{
		return result;
	}
    
    std::clog << "[Scan] scanEntry " << path << std::endl;
	
	const bool isDir = fs::is_directory(path);	
    std::clog << "[Scan] scanEntry " << path << "isDir=" << isDir << std::endl;
	FileRecord newRecord = make_Record(
		NULL_RECORD_ID, 
		FileInfo{ parentID, /*last write time*/0, isDir, path.string() });
    
	const std::pair<FileRecords::iterator, FileRecords::iterator> oldRange = 
		std::equal_range(oldRecords.begin(), oldRecords.end(), newRecord, CmpByPath());
	assert(std::distance(oldRange.first, oldRange.second) <= 1);
	
	const FileRecords::iterator itOldRecord = oldRange.first != oldRange.second ?
		oldRange.first : oldRecords.end();

	if (isDir && recursive)
	{
		// if new entry
		if (itOldRecord == oldRecords.end())
		{
            result.addEntry(std::move(newRecord.second));
		}
	}
	else // file
	{
		if (!isSupportedExtension(path))
		{
			return result; // unsupported extension
		}
				
        newRecord.second.lastWriteTime = fs::last_write_time(path);
        
		if (itOldRecord == oldRecords.end())
		{
            result.addEntry(std::move(newRecord.second));
		}
		else if(newRecord.second.lastWriteTime != 
				itOldRecord->second.lastWriteTime)
		{
			newRecord.first = itOldRecord->first;
            result.replaceEntry(std::move(newRecord));
		}
	}
	
	// record was processed, so removing from the list
	if (oldRecords.end() != itOldRecord)
	{
		oldRecords.erase(itOldRecord);
	}
    
    return result;
}
catch(const fs::filesystem_error& ex)
{
	std::cerr << "Failed to process filesystem element " 
			<< path << ": " << ex.what() << std::endl;
	
	// if the entry is inaccessible due to network resource down
	// it shouldn't be deleted from the database
	if (ex.code().value() != ENOENT)
	{
		const FileRecord fakeRecord = make_Record(NULL_RECORD_ID, 
						FileInfo{ NULL_RECORD_ID, 0, false, path.string() });
		const FileRecords::iterator itOldRecord = std::lower_bound(
			oldRecords.begin(), oldRecords.end(), fakeRecord, CmpByPath());
		
		// prevent record from deletion
		if (oldRecords.end() != itOldRecord)
		{
			oldRecords.erase(itOldRecord);
		}
	}
    
    return Changes();
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to process filesystem element " 
			<< path << ": " << ex.what() << std::endl;
    return Changes();
}


bool ScanThread::isSupportedExtension(const fs::path& fileName)
{
	return extensions_.find(fileName.extension().string()) != extensions_.end();
}

ScanThread::Changes ScanThread::checkDir(const FileRecord& recDir)
try
{
    const fs::path dirPath = recDir.second.fileName;
    Changes result;
    
    std::clog << "[Scan] checkDir " << recDir.second.fileName << std::endl;
            
    if(fs::is_directory(dirPath))
    {              
        time_t const lastWriteTime = fs::last_write_time(dirPath);

        if(lastWriteTime != recDir.second.lastWriteTime)
        {
            FileInfo newData = recDir.second;
            
            newData.lastWriteTime = lastWriteTime;
            result.replaceEntry(make_Record(recDir.first, std::move(newData)));
            
            std::clog << recDir.second.fileName << " changed, scanning" << std::endl;
            result += scanDir(recDir.first, 
                    boost::make_iterator_range(
                        fs::directory_iterator(dirPath), fs::directory_iterator()));
        }
    }
    else
    {
        result.delEntry(recDir.first);
    }
    
    return result;
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to check dir '" 
			<< recDir.second.fileName << "': " << ex.what() << std::endl;
    return Changes();
}


bool ScanThread::Changes::empty() const
{
    return deleted.empty() && changed.empty() && added.empty();
}


void ScanThread::Changes::delEntry(const RecordID& id)
{
    std::clog << "[Scan] delEntry " << id << std::endl;
    deleted.push_back(id);
}


void ScanThread::Changes::addEntry(FileInfo&& data)
{
    std::clog << "[Scan] addEntry " << data.fileName << std::endl;
    added.push_back(std::move(data));
}


void ScanThread::Changes::replaceEntry(FileRecord&& record)
{
    std::clog << "[Scan] replaceEntry " << record.second.fileName << std::endl;
    changed.push_back(std::move(record));
}

void ScanThread::operator() ()
try
{
	std::clog << "Scanning thread started" << std::endl;
    bool hasChanged = true;
	
	while (!stop_)
	{
        if (restart_)
		{ // initially scan directories specified in settings
            std::clog << "[Scan] initial scan " << std::endl;
			auto dirs = settings_.getSettings().directories;
            restart_ = false;
            activeFiles_->onChanged = ActiveRecords::OnChanged();
            continue_ = false;
            hasChanged = true;
			           
            try
            {
                auto changes = scanDir(ROOT_RECORD_ID, dirs);
                save(std::move(changes));
            }
            catch(std::exception const& ex)
            {
                std::cerr << "Error scanning root directories: " 
                    << ex.what() << std::endl;
            }
            
            activeFiles_->onChanged = 
                    std::bind(&ScanThread::onActiveFilesChanged, this, pl::_1);
		}
		
        hasChanged = save(scanDirs(/*isIdle*/!hasChanged));
        
        constexpr static int sleepMs = 500;
        constexpr static int maxSleepMs = 300000; 
        static int           sleepTimeMs = sleepMs;
        
		if (!hasChanged && !stop_)
		{
			struct FackeLock 
			{
				void lock() {}
				void unlock() {}
			} fackeLock;
            
            std::clog << "[Scan] Pause for " << sleepTimeMs << " msec" << std::endl;
            
            for (int i = 0; i < sleepTimeMs; i += sleepMs)
            {
                cond_.wait_for(fackeLock, std::chrono::milliseconds(sleepMs));
                
                if (shouldBreak()) break;
                
                if (!eventSink_.empty())
                {
                    onChangedDisp_(); // doesn't always gets dispatched, so need to push periodically
                }
            }
			
			if (sleepTimeMs < maxSleepMs) // don't sleep more than 5 minutes
			{
			    // next iteration will wait twice as longer
                sleepTimeMs *= 2;
                if (sleepTimeMs > maxSleepMs) sleepTimeMs = maxSleepMs;
            }
		}
		else
		{
			sleepTimeMs = sleepMs;
			std::this_thread::yield();
		}
        
        if (!eventSink_.empty())
        {
            onChangedDisp_();
        }
        
        continue_ = false;
	}
	
	std::clog << "Scanning thread stopped" << std::endl;
}
catch(const std::exception& ex)
{
	std::cerr << "Error in the file scan thread: " 
			<< ex.what() << std::endl;
}
catch(...)
{
	std::cerr << "Unexpected error in the file scan thread: " << std::endl;
}


bool ScanThread::shouldBreak() const
{
	return stop_ || restart_ || continue_;
}


ScanThread::Changes ScanThread::scanDirs(bool isIdle)
{
    Changes changes;
    
    if (isIdle)
    {
        auto const dirIds = activeFiles_->ids;

        for (auto dirId : dirIds)
        {
            if (shouldBreak())
            {
                break;
            }

            try
            {
                auto dir = db_.getFile(dirId);
                changes += checkDir(make_Record(dirId, std::move(dir)));
            }
            catch(std::out_of_range const& e) // directory not in db already (yet)
            {
                std::clog << "[Scan] scanDirs #" << dirId << ": " << e.what() << std::endl;
            }
        }
    }
    else
    {
        for (auto const& dir : db_.dirs())
        {
            changes += checkDir(dir);
        }
    }
    
    return changes;
}


bool ScanThread::save(Changes&& changes)
{
    if (changes.empty())
    {
        return false;
    }
    
    db_.beginTransaction();
    bool succeed = false;
    
    BOOST_SCOPE_EXIT(&db_, &succeed)
    {
        if (succeed)
        {
            db_.commit();
        }
        else
        {
            db_.rollback();
        }
    } BOOST_SCOPE_EXIT_END
    
    for (FileInfo& data : changes.added)
    {
        if (shouldBreak()) break;
        
        RecordID id = db_.addFile(std::move(data));
        eventSink_.push(ScanEvent{ ScanEvent::ADDED, id });
    }
    
    for (FileRecord& record : changes.changed)
    {
        if (shouldBreak()) break;
        
        db_.replaceFile(record.first, std::move(record.second));
        eventSink_.push(ScanEvent{ ScanEvent::UPDATED, record.first });
    }
    
    for (RecordID id : changes.deleted)
    {
        if (shouldBreak()) break;
        
        db_.delFile(id);
    	eventSink_.push(ScanEvent{ ScanEvent::DELETED, id });
    }
    
    succeed = true;
    return true;
}

ScanThread::Changes& ScanThread::Changes::operator+= (Changes&& other)
{
    deleted.splice(deleted.end(), std::move(other.deleted));
    changed.splice(changed.end(), std::move(other.changed));
    added.splice(added.end(), std::move(other.added));
    
    return *this;
}
