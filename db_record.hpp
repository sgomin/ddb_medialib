#ifndef DB_RECORD_HPP
#define DB_RECORD_HPP

#include <string>
#include <ctime>
#include <vector>

#include <stdint.h>

typedef int64_t RecordID;

const RecordID NULL_RECORD_ID = 0;
const RecordID ROOT_RECORD_ID = NULL_RECORD_ID;


struct FileInfo
{
    RecordID    parentID;
    std::time_t lastWriteTime;
	bool		isDir;
    std::string fileName;
};

using FileRecord = std::pair<RecordID, FileInfo>;

template<typename RecordIDT, typename RecordDataT>
inline FileRecord make_Record(RecordIDT&& id, RecordDataT&& data)
{
    return std::make_pair(std::forward<RecordIDT>(id), 
						  std::forward<RecordDataT>(data));
}

using FileRecords = std::vector<FileRecord>;
#endif /* DB_RECORD_HPP */
