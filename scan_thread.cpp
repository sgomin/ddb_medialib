#include "scan_thread.hpp"
#include "database.hpp"

#include <boost/range.hpp>

#include <thread>

ScanThread::ScanThread(
		const Settings::Directories& dirs,
		Database& db)
 : dirs_(dirs)
 , db_(db)
 , changed_(false)
{
}
    
ScanThread::~ScanThread()
{
	
}
    
void ScanThread::operator() ()
try
{
//	while (!stop_)
//	{
		changed_ = false;
		
		for (const std::pair<std::string, Settings::Directory>& dir : dirs_)
		{
			const fs::path path = dir.first;

			if (fs::is_directory(path))
			{
				scanDir(path, RecordID(), dir.second.recursive);
			}
			else
			{
				scanFile(path, RecordID());
			}
		}
		
		if (!changed_)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		else
		{
			std::this_thread::yield();
		}
//	}
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

void ScanThread::scanDir(const fs::path& path, const RecordID& parentID, bool recursive)
try
{
	assert(fs::is_directory(path));
	
	boost::optional<IDRecordPair> pRec = db_.find(path.string());
	const RecordID dirId = pRec ? pRec->first : addDir(path, parentID);
	
	IDRecordPairs children = db_.children(dirId);
	
	auto entriesRange = boost::iterator_range<fs::directory_iterator>(
		fs::directory_iterator(path), fs::directory_iterator());
	
	for (const fs::directory_entry& entry : entriesRange)
	{
		if (fs::is_directory(entry))
		{
			if (recursive)
			{
				scanDir(entry, dirId, /*recursive*/true);
			}
			
			continue;
		}
		
		// TODO: check extension
		scanFile(entry, dirId);
	}
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to process directory " 
			<< path << ": " << ex.what() << std::endl;
}

void ScanThread::scanFile(const fs::path& path, const RecordID& parentID)
try
{
	const time_t lastWriteTime = fs::last_write_time(path);
	
	boost::optional<IDRecordPair> pRec = db_.find(path.string());
	assert(!pRec || pRec->second.header.fileName == path.string());
	
	if (!pRec)
	{
		addFile(path, parentID, lastWriteTime);
		changed_ = true;
	}
	else if (pRec->second.header.lastWriteTime != lastWriteTime)
	{
		pRec->second.header.lastWriteTime = lastWriteTime;
		updateFile(pRec->first, pRec->second);
		changed_ = true;
	}
}
catch(const std::exception& ex)
{
	std::cerr << "Failed to process file " 
			<< path << ": " << ex.what() << std::endl;
}

RecordID ScanThread::addDir(const fs::path& path, const RecordID& parentID)
{
	return addFile(path, parentID, 0);
}

RecordID ScanThread::addFile(
		const fs::path& path, const RecordID& parentID, time_t lastWriteTime)
{
	Record record;
	record.header.fileName = path.string();
	record.header.parentID = parentID;
	record.header.lastWriteTime = lastWriteTime;
	
	return db_.add(record);
}

void ScanThread::updateFile(const RecordID& id, Record& record)
{
	db_.replace(id, record);
}