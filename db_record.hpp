#ifndef DB_RECORD_HPP
#define DB_RECORD_HPP

#include <array>
#include <iosfwd>

#include <db_cxx.h>

class RecordID
{
public:
    RecordID() {}
    explicit RecordID(const Dbt& dbRec);
	
	static RecordID nil();

    uint8_t * data() const { return const_cast<uint8_t*>(data_.data()); };
    constexpr static size_t size() { return DB_HEAP_RID_SZ; }
	
private:    
    typedef std::array<uint8_t, DB_HEAP_RID_SZ> DataT;
    DataT data_;
};

bool operator==(RecordID const& left, RecordID const& right);
bool operator!=(RecordID const& left, RecordID const& right);

size_t hash_value(RecordID const& id);

const RecordID NULL_RECORD_ID = RecordID::nil();
const RecordID ROOT_RECORD_ID = NULL_RECORD_ID;


struct RecordData
{
    RecordData();
    explicit RecordData(const Dbt& dbRec);
    RecordData(const RecordID& parentID, 
               time_t lastWriteTime,
			   bool isDir,
               const std::string& fileName);
    
    std::string data() const;
    
    struct Header
    {
        RecordID    parentID;
        time_t      lastWriteTime;
		bool		isDir;
        std::string fileName;
    } header;
};

typedef std::pair<RecordID, RecordData> Record;

template<typename RecordIDT, typename RecordDataT>
inline Record make_Record(RecordIDT&& id, RecordDataT&& data)
{
    return std::make_pair(std::forward<RecordIDT>(id), 
						  std::forward<RecordDataT>(data));
}

std::istream& operator>> (std::istream& strm, RecordID& recordID);
std::ostream& operator<< (std::ostream& strm, const RecordID& recordID);
std::istream& operator>> (std::istream& strm, RecordData::Header& header);
std::ostream& operator<< (std::ostream& strm, const RecordData::Header& header);

#endif /* DB_RECORD_HPP */

