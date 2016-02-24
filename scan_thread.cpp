#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const Settings::Directories& dirs,
		const Extensions& extensions,
		Database& db,
		ScanEventQueue& eventSink)
 : stop_(false)
 , dirs_(dirs)
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
            EntriesIt itEntriesEnd,
            bool recursive)
{
	if (stop_)
	{
		return;
	}
	
	Records oldRecords = db_.children(dirId);
	std::sort(oldRecords.begin(), oldRecords.end(), CmpByPath());
		
	auto entriesRange = boost::make_iterator_range(itEntriesBegin, itEntriesEnd);
	
	for (const auto& entry : entriesRange)
	{
		scanEntry(getPath(entry), dirId, oldRecords, isRecursive(entry));
	}
	
	for (const Record& missing : oldRecords)
	{
		const fs::path& fileName = missing.second.header.fileName;
		const bool isUnsupportedExtension =
			extensions_.find(fileName.extension().string()) == extensions_.end();
		boost::system::error_code ec; 
		
		if (isUnsupportedExtension || 
			(!fs::exists(fileName, ec) && ec.value() == ENOENT))
		{
			changed_ = true;
			db_.del(missing.first);
			eventSink_.push(ScanEvent{ ScanEvent::DELETED, missing.first });
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
	if (stop_)
	{
		return;
	}
	
	const bool isDir = fs::is_directory(path);	
	Record newRecord = make_Record(
		NULL_RECORD_ID, 
		RecordData(parentID, fs::last_write_time(path), isDir, path.string()));

	const Records::iterator itOldRecord = std::lower_bound(
		oldRecords.begin(), oldRecords.end(), newRecord, CmpByPath());

	if (isDir && recursive)
	{
		const RecordID entryId = itOldRecord != oldRecords.cend() ?
			itOldRecord->first : db_.add(newRecord.second);

		// if new entry
		if (itOldRecord == oldRecords.cend())
		{
			eventSink_.push(ScanEvent{ ScanEvent::ADDED, entryId });
		}
		
		scanDir(entryId, 
				fs::directory_iterator(path), 
				fs::directory_iterator(), 
				/*recursive*/ true);
	}
	else // file
	{
		const std::string ext = path.extension().string();
		
		if (extensions_.find(ext) == extensions_.end())
		{
			return; // unsupported extension
		}
				
		if (itOldRecord == oldRecords.cend())
		{
			changed_ = true;
			RecordID newId = db_.add(newRecord.second);
			eventSink_.push(ScanEvent{ ScanEvent::ADDED, newId });
		}
		else if(newRecord.second.header.lastWriteTime != 
				itOldRecord->second.header.lastWriteTime)
		{
			changed_ = true;
			const RecordID& replaceId = itOldRecord->first;
			db_.replace(replaceId, newRecord.second);
			eventSink_.push(ScanEvent{ ScanEvent::DELETED, replaceId });
			eventSink_.push(ScanEvent{ ScanEvent::ADDED, replaceId });
		}
	}
	
	// record was processed, so removing from the list
	if (oldRecords.end() != itOldRecord)
	{
		oldRecords.erase(itOldRecord);
	}
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to process filesystem element " 
			<< path << ": " << ex.what() << std::endl;
}

void ScanThread::operator() ()
try
{
	std::clog << "Starting scanning thread" << std::endl;
	
	while (!stop_)
	{
		changed_ = false;
		
		scanDir(ROOT_RECORD_ID, 
				dirs_.cbegin(), 
				dirs_.cend(), 
				/*recursive*/ true);
		
		if (!changed_ && !stop_)
		{
			struct FackeLock 
			{
				void lock() {}
				void unlock() {}
			} fackeLock;
			
			cond_.wait_for(fackeLock, sleepTime_);
			
			if (sleepTime_ < std::chrono::seconds(2))
			{
				sleepTime_ += std::chrono::milliseconds(100);
			}
		}
		else
		{
			sleepTime_ = std::chrono::milliseconds(100);
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
