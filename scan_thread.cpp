#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const SettingsProvider& settings,
		const Extensions& extensions,
		Database& db,
		ScanEventSink eventSink)
 : stop_(false)
 , restart_(true)
 , settings_(settings)
 , extensions_(extensions)
 , db_(db)
 , eventSink_(eventSink)
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
	bool operator() (const Record& left, const Record& right) const
	{
		return left.second.header.fileName < right.second.header.fileName;
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
	
	Records oldRecords = db_.children(dirId);
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
	
	for (const Record& missing : oldRecords)
	{
		try
		{
            delEntry(missing.first);
		}
		catch(std::exception const& ex)
		{
			std::cerr << "Failed to delete DB record for '" 
					<< missing.second.header.fileName << "' entry: " 
					<< ex.what() << std::endl;
		}
	}
}

void ScanThread::scanEntry(
			const fs::path& path, 
			const RecordID& parentID, 
			Records& oldRecords,
			bool recursive)
try
{
	if (shouldBreak())
	{
		return;
	}
	
	const bool isDir = fs::is_directory(path);	
	Record newRecord = make_Record(
		NULL_RECORD_ID, 
		RecordData(parentID, /*last write time*/0, isDir, path.string()));

	const std::pair<Records::iterator, Records::iterator> oldRange = 
		std::equal_range(oldRecords.begin(), oldRecords.end(), newRecord, CmpByPath());
	assert(std::distance(oldRange.first, oldRange.second) <= 1);
	
	const Records::iterator itOldRecord = oldRange.first != oldRange.second ?
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
				
        newRecord.second.header.lastWriteTime = fs::last_write_time(path);
        
		if (itOldRecord == oldRecords.end())
		{
            addEntry(std::move(newRecord.second));
		}
		else if(newRecord.second.header.lastWriteTime != 
				itOldRecord->second.header.lastWriteTime)
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
		const Record fakeRecord = make_Record(NULL_RECORD_ID, 
						RecordData(NULL_RECORD_ID, 0, false, path.string()));
		const Records::iterator itOldRecord = std::lower_bound(
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

void ScanThread::checkDir(const Record& recDir)
try
{
    const fs::path dirPath = recDir.second.header.fileName;
            
    if(fs::is_directory(dirPath))
    {              
        time_t const lastWriteTime = fs::last_write_time(dirPath);

        if(lastWriteTime != recDir.second.header.lastWriteTime)
        {
            RecordData newData = recDir.second;
            
            newData.header.lastWriteTime = lastWriteTime;
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
			<< recDir.second.header.fileName << "': " << ex.what() << std::endl;
}


void ScanThread::delEntry(const RecordID& id)
{
    changed_ = true;
    changes_.deleted.push_back(id);
}


void ScanThread::addEntry(RecordData&& data)
{
    changed_ = true;
    changes_.added.push_back(std::move(data));
}


void ScanThread::replaceEntry(Record&& record)
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
    db_iterator const itDirsEnd = db_.dirs_end();
        
    for (db_iterator itDir = db_.dirs_begin(); 
         itDir != itDirsEnd && !shouldBreak(); 
                                      ++itDir)
    {
        checkDir(*itDir);
    }
}


void ScanThread::saveChangesToDB()
{
    for (RecordData& data : changes_.added)
    {
        if (shouldBreak()) break;
        
        RecordID id = db_.add(std::move(data));
        eventSink_.push(ScanEvent{ ScanEvent::ADDED, std::move(id) });
    }
    
    for (Record& record : changes_.changed)
    {
        if (shouldBreak()) break;
        
        db_.replace(record.first, std::move(record.second));
        eventSink_.push(ScanEvent{ ScanEvent::UPDATED, std::move(record.first) });
    }
    
    for (RecordID& id : changes_.deleted)
    {
        if (shouldBreak()) break;
        
        db_.del(id);
    	eventSink_.push(ScanEvent{ ScanEvent::DELETED, std::move(id) });
    }
    
    changes_.clear();
}


void ScanThread::Changes::clear()
{
    deleted.clear();
    changed.clear();
    added.clear();
}
