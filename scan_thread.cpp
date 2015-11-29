#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const Settings::Directories& dirs,
		Database& db)
 : stop_(false)
 , dirs_(dirs)
 , db_(db)
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
		boost::system::error_code ec;
		const bool exists = fs::exists(missing.second.header.fileName, ec);
		
		if (!exists && ec.value() == ENOENT)
		{
			changed_ = true;
			db_.del(missing.first);
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
	
	Record newRecord = make_Record(
		RecordID(), 
		RecordData(parentID, fs::last_write_time(path), path.string()));

	const Records::iterator itOldRecord = std::lower_bound(
		oldRecords.begin(), oldRecords.end(), newRecord, CmpByPath());

	if (fs::is_directory(path) && recursive)
	{
		const RecordID entryId = itOldRecord != oldRecords.cend() ?
			itOldRecord->first : db_.add(newRecord.second);

		scanDir(entryId, 
				fs::directory_iterator(path), 
				fs::directory_iterator(), 
				/*recursive*/ true);
	}
	else // file
	{
		// TODO: check extension
		if (itOldRecord == oldRecords.cend())
		{
			changed_ = true;
			db_.add(newRecord.second);
		}
		else if(newRecord.second.header.lastWriteTime != 
				itOldRecord->second.header.lastWriteTime)
		{
			changed_ = true;
			db_.replace(itOldRecord->first, newRecord.second);
		}
	}
	
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
	while (!stop_)
	{
		changed_ = false;
		
		scanDir(RecordID(), 
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
