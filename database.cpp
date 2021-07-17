#include "database.hpp"

#include "sqlite3/sqlite_locked.h"
#include "sqlite3/sqlite3.h"

#include <iostream>
#include <assert.h>

#define CHECK_SQLITE(expr) \
    { auto res = expr; if (res != SQLITE_OK) throw DbException(res); }

DbOwner::DbOwner(const std::string& fileName)
    : DbReader(nullptr)
    , fileName_(fileName)
{
    CHECK_SQLITE(sqlite3_enable_shared_cache(true));
    
    auto res = sqlite3_open_v2(fileName.c_str(), &pDb_, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    
    if (res != SQLITE_OK)
    {
        std::clog << "Failed to open database at " << fileName << std::endl;
        close();
        throw DbException(res);
    }
    
    const char * const szSQL =
    "PRAGMA foreign_keys = ON;"
    "CREATE TABLE IF NOT EXISTS files("
        "id INTEGER PRIMARY KEY ASC,"
        "parent_id INTEGER,"
        "write_time DATETIME,"
        "is_dir BOOLEAN,"
        "name TEXT,"
        "FOREIGN KEY(parent_id) REFERENCES files(id) ON DELETE CASCADE"
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
    
    if (record.parentID != NULL_RECORD_ID)
    {
        CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, record.parentID));
    }
    else
    {
        CHECK_SQLITE(sqlite3_bind_null(pStmt, 1));
    }
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 2, record.lastWriteTime));
    CHECK_SQLITE(sqlite3_bind_int(pStmt, 3, record.isDir ? 1 : 0));
    CHECK_SQLITE(sqlite3_bind_text(pStmt, 4, 
       record.fileName.c_str(), record.fileName.length(), SQLITE_TRANSIENT));
    
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
    
    return sqlite3_last_insert_rowid(pDb_);
}


void DbOwner::delFile(RecordID id)
{
    constexpr const char * const szSQL =
       "DELETE FROM files WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
}


void DbOwner::replaceFile(RecordID id, const FileInfo& record)
{
    constexpr const char * const szSQL =
       "UPDATE files SET"
       " parent_id = :parent_id,"
       " write_time = :write_time,"
       " is_dir = :is_dir,"
       " name = :name"
       " WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    if (record.parentID != NULL_RECORD_ID)
    {
        CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, record.parentID));
    }
    else
    {
        CHECK_SQLITE(sqlite3_bind_null(pStmt, 1));
    }
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 2, record.lastWriteTime));
    CHECK_SQLITE(sqlite3_bind_int(pStmt, 3, record.isDir ? 1 : 0));
    CHECK_SQLITE(sqlite3_bind_text(pStmt, 4, 
       record.fileName.c_str(), record.fileName.length(), SQLITE_TRANSIENT));
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 5, id));
    
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
}
    

void DbOwner::beginTransaction()
{
    sqlite3_stmt * pStmt = statements_.get(__LINE__, "BEGIN TRANSACTION");
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        throw DbException(res);
    }
}


void DbOwner::commit()
{
    sqlite3_stmt * pStmt = statements_.get(__LINE__, "COMMIT TRANSACTION");
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        std::cerr << "Failed to commit transaction";
    }
}

    
void DbOwner::rollback()
{
    sqlite3_stmt * pStmt = statements_.get(__LINE__, "ROLLBACK TRANSACTION");
    auto res = sqlite3_blocking_step(pStmt);
    
    if (res != SQLITE_DONE)
    {
        std::cerr << "Failed to rollback transaction";
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


FileInfo DbReader::getFile(RecordID id) const
{
    constexpr const char * const szSQL =
       "SELECT parent_id, write_time, is_dir, name"
       " FROM files WHERE id = :id";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    auto res = sqlite3_blocking_step(pStmt);
    
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


FileRecords DbReader::childrenFiles(RecordID id) const
{
    constexpr const char * const szSQL =
       "SELECT id, parent_id, write_time, is_dir, name"
       " FROM files"
       " WHERE parent_id = :parent_id"
         " OR (parent_id IS NULL AND :parent_id IS NULL)";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    if (id)
    {
        CHECK_SQLITE(sqlite3_bind_int64(pStmt, 1, id));
    }
    else
    {
        CHECK_SQLITE(sqlite3_bind_null(pStmt, 1));
    }
    
    FileRecords result;
    
    while (auto rec = readNextRecord(pStmt))
    {
        result.push_back(std::move(*rec));
    }
    
    return result;
}   


FileRecords DbReader::dirs() const
{
    constexpr const char * const szSQL =
       "SELECT id, parent_id, write_time, is_dir, name"
       " FROM files WHERE is_dir";
    
    sqlite3_stmt * pStmt = statements_.get(__LINE__, szSQL);
    
    FileRecords result;
    
    while (auto rec = readNextRecord(pStmt))
    {
        result.push_back(std::move(*rec));
    }
    
    return result;
}


std::optional<FileRecord> DbReader::readNextRecord(sqlite3_stmt* pStmt)
{
    assert(pStmt);
    
    auto const res = sqlite3_blocking_step(pStmt);
    
    if (res == SQLITE_DONE)
    {
        CHECK_SQLITE(sqlite3_reset(pStmt));
        return std::nullopt;
    }
    else if (res != SQLITE_ROW)
    {
        throw DbException(res);
    }
    else
    {
        FileRecord rec;
        
        rec.first = sqlite3_column_int64(pStmt, 0);
        rec.second.parentID = sqlite3_column_int64(pStmt, 1);
        rec.second.lastWriteTime = sqlite3_column_int64(pStmt, 2);
        rec.second.isDir = sqlite3_column_int(pStmt, 3) != 0;
        auto const * pFileName = sqlite3_column_text(pStmt, 4);

        if (pFileName)
        {
            rec.second.fileName = reinterpret_cast<const char*>(pFileName);
        }
        else
        {
            rec.second.fileName.clear();
        }
        
        return rec;
    }
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
        CHECK_SQLITE(sqlite3_blocking_prepare_v2(pDb_, szSQL, -1, &pStmt, nullptr));
    }
    else
    {
//        assert(sqlite3_stmt_busy(pStmt) == 0);
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