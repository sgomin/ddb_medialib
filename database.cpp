#include "database.hpp"

#include "sqlite3/sqlite3.h"

#include <iostream>

namespace 
{
    
}

#define CHECK_SQLITE(expr) \
    { auto res = expr; if (res != SQLITE_OK) throw DbException(res); }

DbOwner::DbOwner(const std::string& fileName)
    : DbReader(nullptr)
    , fileName_(fileName)
{
    auto res = sqlite3_open_v2(fileName.c_str(), &pDb_, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    
    if (res != SQLITE_OK)
    {
        std::clog << "Failed to open database at " << fileName << std::endl;
        close();
        throw DbException(res);
    }
    
    const char * const szSQL =
    "CREATE TABLE IF NOT EXISTS files("
        "id INTEGER PRIMARY KEY ASC,"
        "parent_id INTEGER REFERENCES files(id) ON DELETE CASCADE,"
        "write_time DATETIME,"
        "is_dir BOOLEAN,"
        "name TEXT"
        ");";
    
    res = sqlite3_exec(pDb_, szSQL, nullptr, nullptr, nullptr);
    
    if (res != SQLITE_OK)
    {
        std::clog << "Failed to create database schema" << std::endl;
        throw DbException(res);
    }
    
    statements_.setDb(pDb_);
}


RecordID DbOwner::addFile(const FileInfo& record)
{
    constexpr const char * const szSQL =
       "INSERT INTO files (parent_id, write_time, is_dir, name)"
       " VALUES(:parent_id, :write_time, :is_dir, :name)";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, record.parentID));
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 2, record.lastWriteTime));
    CHECK_SQLITE(sqlite3_bind_int(pStmt, 3, record.isDir ? 1 : 0));
    CHECK_SQLITE(sqlite3_bind_text(pStmt, 4, 
       record.fileName.c_str(), record.fileName.length(), SQLITE_TRANSIENT));
    
    auto res = sqlite3_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
    
    return sqlite3_last_insert_rowid(pDb_);
}


void DbOwner::delFile(const RecordID& id)
{
    constexpr const char * const szSQL =
       "DELETE FROM files WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    auto res = sqlite3_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
}


void DbOwner::replaceFile(const RecordID& id, const FileInfo& record)
{
    constexpr const char * const szSQL =
       "UPDATE files SET"
       " parent_id = :parent_id,"
       " write_time = :write_time,"
       " is_dir = :is_dir,"
       " name = :name"
       " WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, record.parentID));
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 2, record.lastWriteTime));
    CHECK_SQLITE(sqlite3_bind_int(pStmt, 3, record.isDir ? 1 : 0));
    CHECK_SQLITE(sqlite3_bind_text(pStmt, 4, 
       record.fileName.c_str(), record.fileName.length(), SQLITE_TRANSIENT));
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 5, id));
    
    auto res = sqlite3_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
}
    
    
DbReader DbOwner::createReader()
{
    sqlite3* pDb = nullptr;
    
    auto res = sqlite3_open_v2(
        fileName_.c_str(), &pDb, SQLITE_OPEN_READONLY, nullptr);
    
    DbReader reader(pDb);
    
    if (res != SQLITE_OK)
    {
        std::clog << "Failed to open read-only database at " << fileName_ << std::endl;
        throw DbException(res);
    }
    
    return reader;
}


// --- DbReader ----------------------------------------------------------------

DbReader::DbReader(sqlite3* pDb) 
    : pDb_(pDb)
    , statements_(pDb)
{
}


DbReader::DbReader(DbReader&& other)
    : pDb_(other.pDb_)
    , statements_(std::move(other.statements_))
{
    other.pDb_ = nullptr;
}


DbReader::~DbReader()
{
    close();
}


void DbReader::close()
{
    if (!pDb_)
    {
        return;
    }
    
    statements_.clear();

    auto res = sqlite3_close_v2(pDb_);

    pDb_ = nullptr;

    if (res != SQLITE_OK)
    {
        std::clog << "Database closed with error: " << sqlite3_errstr(res) 
                << std::endl;
    }
}


FileInfo DbReader::getFile(const RecordID& id) const
{
    constexpr const char * const szSQL =
       "SELECT parent_id, write_time, is_dir, name"
       " FROM files WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    auto res = sqlite3_step(pStmt);
    
    if (res == SQLITE_DONE)
    {
        throw std::out_of_range("No such file record");
    }
    else if (res != SQLITE_ROW)
    {
        throw DbException(res);
    }
    
    FileInfo rec;
    
    rec.parentID = sqlite3_column_int64(pStmt, 0);
    rec.lastWriteTime = sqlite3_column_int64(pStmt, 1);
    rec.isDir = sqlite3_column_int(pStmt, 2) != 0;
    auto const * pFileName = sqlite3_column_text(pStmt, 3);
    
    if (pFileName)
    {
        rec.fileName = reinterpret_cast<const char*>(pFileName);
    }
    
    CHECK_SQLITE(sqlite3_reset(pStmt));
    return rec;
}


db_file_iterator_range DbReader::childrenFiles(const RecordID& id) const
{
    constexpr const char * const szSQL =
       "SELECT id, parent_id, write_time, is_dir, name"
       " FROM files WHERE parent_id = :parent_id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    
    return boost::make_iterator_range(
            db_file_iterator(pStmt), db_file_iterator(nullptr));
}   


db_file_iterator_range DbReader::dirs() const
{
    constexpr const char * const szSQL =
       "SELECT id, parent_id, write_time, is_dir, name"
       " FROM files WHERE is_dir";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    return boost::make_iterator_range(
            db_file_iterator(pStmt), db_file_iterator(nullptr));
}


// --- StatementCache ----------------------------------------------------------

StatementCache::StatementCache(sqlite3* pDb)
    : pDb_(pDb)
{
    
}

StatementCache::~StatementCache()
{
    clear();
}
    

void StatementCache::setDb(sqlite3* pDb)
{
    pDb_ = pDb;
}


sqlite3_stmt* StatementCache::get(int id, const char* szSQL)
{
    auto& pStmt = statements_[id];
    
    if (!pStmt)
    {
        CHECK_SQLITE(sqlite3_prepare_v2(pDb_, szSQL, -1, &pStmt, nullptr));
    }
    else
    {
        assert(sqlite3_stmt_busy(pStmt) == 0);
        CHECK_SQLITE(sqlite3_reset(pStmt));
    }
    
    return pStmt;
}
    

void StatementCache::clear()
{
    for (auto stmt : statements_)
    {
        sqlite3_finalize(stmt.second);
    }
    
    statements_.clear();
}

// --- DbException -------------------------------------------------------------

DbException::DbException(int err)
    : std::runtime_error(sqlite3_errstr(err))
{
}