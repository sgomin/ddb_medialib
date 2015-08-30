#ifndef DATABASE_HPP
#define	DATABASE_HPP

#include <boost/optional.hpp>

#include <iosfwd>
#include <array>

#include <db_cxx.h>

class RecordID
{
public:
    RecordID();
    explicit RecordID(const Dbt& dbRec);

    uint8_t * data() const { return const_cast<uint8_t*>(data_.data()); };
    constexpr static size_t size() { return DB_HEAP_RID_SZ; }
 
private:    
    typedef std::array<uint8_t, DB_HEAP_RID_SZ> DataT;
    DataT data_;
};

struct Record
{
    Record();
    explicit Record(const Dbt& dbRec);
    
    std::string data() const;
    
    struct Header
    {
        RecordID    parentID;
        time_t      lastWriteTime;
        std::string fileName;
    } header;
};

typedef std::pair<RecordID, Record> IDRecordPair;
typedef std::vector<IDRecordPair> IDRecordPairs;

std::istream& operator>> (std::istream& strm, RecordID& recordID);
std::ostream& operator<< (std::ostream& strm, const RecordID& recordID);
std::istream& operator>> (std::istream& strm, Record::Header& header);
std::ostream& operator<< (std::ostream& strm, const Record::Header& header);

class Database
{
public:
    Database();
    
    void            open(const std::string& path);
    Record          get(const RecordID& id) const;
    RecordID        add(const Record& record);
    void            replace(const RecordID& id, const Record& record);
    IDRecordPairs   children(const RecordID& idParent) const;
    boost::optional<IDRecordPair> find(const std::string& fileName) const;
    
private:
    static int getFileName(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    static int getParentId(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    
    static std::string convert(const Record& record);

private:
    DbEnv       env_;
	mutable Db	dbMain_;
    mutable Db  dbFilename_;
    mutable Db  dbParentId_;
};

#endif	/* DATABASE_HPP */

