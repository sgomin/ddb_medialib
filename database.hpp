#ifndef DATABASE_HPP
#define	DATABASE_HPP

#include "db_record.hpp"
#include "db_iterator.hpp"

#include <boost/noncopyable.hpp>

#include <string>
#include <memory>
#include <stdexcept>
#include <unordered_map>

struct sqlite3;
struct sqlite3_stmt;


class StatementCache
{
public:
    explicit StatementCache(sqlite3* pDb);
    ~StatementCache();
    
    sqlite3_stmt* get(int id, const char* szSQL);
    void clear();
    void setDb(sqlite3* pDb);
    
private:
    sqlite3*                               pDb_;
    std::unordered_map<int, sqlite3_stmt*> statements_;
};


class DbReader
{
public:
    ~DbReader();
    DbReader(DbReader const&) = delete;
    DbReader(DbReader&& other);
    
    DbReader& operator=(DbReader const&) = delete;
    
    FileInfo    getFile(const RecordID& id) const;
    db_file_iterator_range childrenFiles(const RecordID& id) const;
    db_file_iterator_range dirs() const;
    
protected:
    friend class DbOwner;
    DbReader(sqlite3* pDb);
    
    void close();
    
    sqlite3*                pDb_;
    mutable StatementCache  statements_;
};


class DbOwner : public DbReader, private boost::noncopyable
{
public:
    explicit DbOwner(const std::string& fileName);
    
    RecordID    addFile(const FileInfo& record);
    void        delFile(const RecordID& id);
    void        replaceFile(const RecordID& id, const FileInfo& record);
    
    DbReader createReader();
    
private:    
    const std::string fileName_;
};

using DbOwnerPtr = std::unique_ptr<DbOwner>;

class DbException : public std::runtime_error
{
public:
    explicit DbException(int err);
};

#endif	/* DATABASE_HPP */

