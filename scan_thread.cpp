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


template<typename EntriesIt>
void ScanThread::scanDir(
            const RecordID& dirId, 
            EntriesIt itEntriesBegin, 
            EntriesIt itEntriesEnd)
{
	if (shouldBreak())
	{
		return;
	}
	
    auto rangeOldRecords = db_->childrenFiles(dirId);
	FileRecords oldRecords{ rangeOldRecords.begin(), rangeOldRecords.end() };
    
	std::sort(oldRecords.begin(), oldRecords.end(), CmpByPath());
		
	auto entriesRange = boost::make_iterator_range(itEntriesBegin, itEntriesEnd);
	
	for (const auto& entry : entriesRange)
	{
		scanEntry(getPath(entry), dirId, oldRecords, isRecursive(entry));
		
		if (shouldBreak())
		{
			return;
		}
	}
	
	for (const FileRecord& missing : oldRecords)
	{
		try
		{
            delEntry(missing.first);
		}
		catch(std::exception const& ex)
		{
			std::cerr << "Failed to delete DB record for '" 
					<< missing.second.fileName << "' entry: " 
					<< ex.what() << std::endl;
		}
	}
}

void ScanThread::scanEntry(
			const fs::path& path, 
			const RecordID& parentID, 
			FileRecords& oldRecords,
			bool recursive)
try
{
	if (shouldBreak())
	{
		return;
	}
	
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
            addEntry(std::move(newRecord.second));
		}
	}
	else // file
	{
		if (!isSupportedExtension(path))
		{
			return; // unsupported extension
		}
				
        newRecord.second.lastWriteTime = fs::last_write_time(path);
        
		if (itOldRecord == oldRecords.end())
		{
            addEntry(std::move(newRecord.second));
		}
		else if(newRecord.second.lastWriteTime != 
				itOldRecord->second.lastWriteTime)
		{
			newRecord.first = itOldRecord->first;
            replaceEntry(std::move(newRecord));
		}
	}
	
	// record was processed, so removing from the list
	if (oldRecords.end() != itOldRecord)
	{
		oldRecords.erase(itOldRecord);
	}
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
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to process filesystem element " 
			<< path << ": " << ex.what() << std::endl;
}


bool ScanThread::isSupportedExtension(const fs::path& fileName)
{
	return extensions_.find(fileName.extension().string()) != extensions_.end();
}

void ScanThread::checkDir(const FileRecord& recDir)
try
{
    const fs::path dirPath = recDir.second.fileName;
            
    if(fs::is_directory(dirPath))
    {              
        time_t const lastWriteTime = fs::last_write_time(dirPath);

        if(lastWriteTime != recDir.second.lastWriteTime)
        {
            FileInfo newData = recDir.second;
            
            newData.lastWriteTime = lastWriteTime;
            replaceEntry(make_Record(recDir.first, std::move(newData)));
            
            scanDir(recDir.first, 
                    fs::directory_iterator(dirPath), 
                    fs::directory_iterator());
        }
    }
    else
    {
        delEntry(recDir.first);
    }
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to check dir '" 
			<< recDir.second.fileName << "': " << ex.what() << std::endl;
}


void ScanThread::delEntry(const RecordID& id)
{
    changed_ = true;
    changes_.deleted.push_back(id);
}


void ScanThread::addEntry(FileInfo&& data)
{
    changed_ = true;
    changes_.added.push_back(std::move(data));
}


void ScanThread::replaceEntry(FileRecord&& record)
{
    changed_ = true;
    changes_.changed.push_back(std::move(record));
}

void ScanThread::operator() ()
try
{
	std::clog << "Scanning thread started" << std::endl;
	
	while (!stop_)
	{
        changed_ = false;
        
		if (restart_)
		{ // initially scan directories specified in settings
			Settings::Directories dirs = settings_.getSettings().directories;
            restart_ = false;
			           
            try
            {
                scanDir(ROOT_RECORD_ID, dirs.cbegin(), dirs.cend());
                saveChangesToDB();
            }
            catch(std::exception const& ex)
            {
                std::cerr << "Error scanning root directories: " 
                    << ex.what() << std::endl;
            }
		}
		
        scanDirs();
        saveChangesToDB();
        
		if (!changed_ && !stop_)
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
			sleepTime_ = std::chrono::seconds(2);
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


void ScanThread::scanDirs()
{
    for (auto dir : db_->dirs())
    {
        if (shouldBreak())
        {
            break;
        }
        
        checkDir(dir);
    }
}


void ScanThread::saveChangesToDB()
{
    for (FileInfo& data : changes_.added)
    {
        if (shouldBreak()) break;
        
        RecordID id = db_->addFile(std::move(data));
        eventSink_.push(ScanEvent{ ScanEvent::ADDED, std::move(id) });
    }
    
    for (FileRecord& record : changes_.changed)
    {
        if (shouldBreak()) break;
        
        db_->replaceFile(record.first, std::move(record.second));
        eventSink_.push(ScanEvent{ ScanEvent::UPDATED, std::move(record.first) });
    }
    
    for (RecordID& id : changes_.deleted)
    {
        if (shouldBreak()) break;
        
        db_->delFile(id);
    	eventSink_.push(ScanEvent{ ScanEvent::DELETED, std::move(id) });
    }
    
    if (shouldBreak()) return;
    
    changes_.clear();
    
    if (!eventSink_.empty())
    {
        onChangedDisp_();
    }
}


void ScanThread::Changes::clear()
{
    deleted.clear();
    changed.clear();
    added.clear();
}
