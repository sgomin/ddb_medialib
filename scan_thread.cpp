#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const SettingsProvider& settings,
		const Extensions& extensions,
		Database& db,
		ScanEventQueue& eventSink)
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
            EntriesIt itEntriesEnd,
            bool recursive,
			bool forceDeleteMissing)
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
	}
	
	for (const Record& missing : oldRecords)
	{
		changed_ = true;
		db_.del(missing.first);
		eventSink_.push(ScanEvent{ ScanEvent::DELETED, missing.first });
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
				/*recursive*/ true,
				/*forceDeleteMissing*/ false);
	}
	else // file
	{
		if (!isSupportedExtension(path))
		{
			return; // unsupported extension
		}
				
		if (itOldRecord == oldRecords.cend())
		{
			changed_ = true;
			newRecord.first = db_.add(newRecord.second);
			eventSink_.push(ScanEvent{ ScanEvent::ADDED, newRecord.first });
		}
		else if(newRecord.second.header.lastWriteTime != 
				itOldRecord->second.header.lastWriteTime)
		{
			changed_ = true;
			newRecord.first = itOldRecord->first;
			db_.replace(newRecord.first, newRecord.second);
			eventSink_.push(ScanEvent{ ScanEvent::UPDATED, newRecord.first });
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

void ScanThread::operator() ()
try
{
	std::clog << "Scanning thread started" << std::endl;
	
	Settings::Directories dirs;
	
	while (!stop_)
	{
		if (restart_)
		{
			dirs = settings_.getSettings().directories;
			restart_ = false;
		}
		
		changed_ = false;
		
		scanDir(ROOT_RECORD_ID, 
				dirs.cbegin(), 
				dirs.cend(), 
				/*recursive*/ true,
				/*forceDeleteMissing*/ true);
		
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


bool ScanThread::shouldBreak() const
{
	return stop_ || restart_;
}