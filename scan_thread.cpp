#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const SettingsProvider& settings,
		const Extensions& extensions,
		DbOwnerPtr&& db,
		ScanEventSink eventSink,
        Glib::Dispatcher& onChangedDisp)
 : stop_(false)
 , restart_(true)
 , settings_(settings)
 , extensions_(extensions)
 , db_(std::move(db))
 , eventSink_(eventSink)
 , onChangedDisp_(onChangedDisp)
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
	
    auto oldRecords = db_->childrenFiles(dirId);
    
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
	
	while (!stop_)
	{
        if (restart_)
		{ // initially scan directories specified in settings
            std::clog << "[Scan] initial scan " << std::endl;
			Settings::Directories dirs = settings_.getSettings().directories;
            restart_ = false;
			           
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
		}
		
        auto changed = save(scanDirs());
        
		if (!changed && !stop_)
		{
			struct FackeLock 
			{
				void lock() {}
				void unlock() {}
			} fackeLock;
			
			cond_.wait_for(fackeLock, sleepTime_);
			
			if (sleepTime_ < std::chrono::seconds(5))
			{
				sleepTime_ += std::chrono::seconds(1);
            }
		}
		else
		{
			sleepTime_ = std::chrono::seconds(1);
			std::this_thread::yield();
		}
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
	return stop_ || restart_;
}


ScanThread::Changes ScanThread::scanDirs()
{
    Changes changes;
    
    for (auto dir : db_->dirs())
    {
        if (shouldBreak())
        {
            break;
        }
        
        changes += checkDir(dir);
    }
    
    return changes;
}


bool ScanThread::save(Changes&& changes)
{
    bool changed = !changes.empty();
    
    for (FileInfo& data : changes.added)
    {
        if (shouldBreak()) break;
        
        RecordID id = db_->addFile(std::move(data));
        eventSink_.push(ScanEvent{ ScanEvent::ADDED, std::move(id) });
    }
    
    for (FileRecord& record : changes.changed)
    {
        if (shouldBreak()) break;
        
        db_->replaceFile(record.first, std::move(record.second));
        eventSink_.push(ScanEvent{ ScanEvent::UPDATED, std::move(record.first) });
    }
    
    for (RecordID id : changes.deleted)
    {
        if (shouldBreak()) break;
        
        db_->delFile(id);
    	eventSink_.push(ScanEvent{ ScanEvent::DELETED, std::move(id) });
    }
    
    if (!eventSink_.empty())
    {
        onChangedDisp_();
    }
    
    return changed;
}

ScanThread::Changes& ScanThread::Changes::operator+= (Changes&& other)
{
    deleted.splice(deleted.end(), std::move(other.deleted));
    changed.splice(changed.end(), std::move(other.changed));
    added.splice(added.end(), std::move(other.added));
    
    return *this;
}
