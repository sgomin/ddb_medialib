#include "db_iterator.hpp"
#include "database.hpp"

#include "sqlite3/sqlite3.h"
#include <assert.h>


db_file_iterator::db_file_iterator(struct sqlite3_stmt* pCursor)
    : pCursor_(pCursor)
{
    readNextRecord();
}


db_file_iterator::~db_file_iterator()
{
    
}


void db_file_iterator::increment()
{
    assert(pCursor_);
    readNextRecord();
}
   

bool db_file_iterator::equal(db_file_iterator const& other) const
{
    return pCursor_ == other.pCursor_;
}


FileRecord const& db_file_iterator::dereference() const 
{ 
    assert(pCursor_);
    return rec_;   
}


void db_file_iterator::readNextRecord()
{
    if (!pCursor_)
    {
        return;
    }
    
    auto res = sqlite3_step(pCursor_);
    
    if (res == SQLITE_DONE)
    {
        pCursor_ = nullptr;
    }
    else if (res != SQLITE_ROW)
    {
        throw DbException(res);
    }
    
    rec_.first = sqlite3_column_int64(pCursor_, 0);
    rec_.second.parentID = sqlite3_column_int64(pCursor_, 1);
    rec_.second.lastWriteTime = sqlite3_column_int64(pCursor_, 2);
    rec_.second.isDir = sqlite3_column_int(pCursor_, 3) != 0;
    auto const * pFileName = sqlite3_column_text(pCursor_, 4);
    
    if (pFileName)
    {
        rec_.second.fileName = reinterpret_cast<const char*>(pFileName);
    }
    else
    {
        rec_.second.fileName.clear();
    }
}