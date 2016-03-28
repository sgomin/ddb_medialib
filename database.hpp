#ifndef DATABASE_HPP
#define	DATABASE_HPP

#include <iosfwd>
#include <array>
#include <vector>

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

size_t hash_value(RecordID const& id);

const RecordID NULL_RECORD_ID = RecordID::nil();
const RecordID ROOT_RECORD_ID = NULL_RECORD_ID;


struct RecordData
{
    RecordData();
    RecordData(RecordData&& other) = default;
    explicit RecordData(const Dbt& dbRec);
    RecordData(const RecordID& parentID, 
               time_t lastWriteTime,
			   bool isDir,
               const std::string& fileName);
    
    RecordData& operator=(RecordData&& other) = default;
    
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

typedef std::vector<Record> Records;

std::istream& operator>> (std::istream& strm, RecordID& recordID);
std::ostream& operator<< (std::ostream& strm, const RecordID& recordID);
std::istream& operator>> (std::istream& strm, RecordData::Header& header);
std::ostream& operator<< (std::ostream& strm, const RecordData::Header& header);

class Database
{
public:
    Database();
    
    /// Opens database at given directory, creates if doesn't exist 
    void        open(const std::string& path);
	void		close();
    
    RecordData  get(const RecordID& id) const;
    RecordID    add(const RecordData& record);
    void        del(const RecordID& id);
    void        replace(const RecordID& id, const RecordData& record);
    Records     children(const RecordID& idParent) const;
    Record      find(const std::string& fileName) const;
    
private:
    static int getFileName(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    static int getParentId(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    
    static std::string convert(const RecordData& record);

private:
    DbEnv       env_;
	mutable Db	dbMain_;
    mutable Db  dbFilename_;
    mutable Db  dbParentId_;
};

#endif	/* DATABASE_HPP */

