#include "db_iterator.hpp"


#include <db_cxx.h>
#include <assert.h>

db_iterator::db_iterator(Dbc* pCursor, Record&& record)
    : pCursor_(pCursor)
    , record_(std::move(record))
{
}


db_iterator::db_iterator(const db_iterator& orig)
    : pCursor_(nullptr)
    , record_(orig.record_)
{
    if (orig.pCursor_)
    {
        orig.pCursor_->dup(&pCursor_, DB_POSITION);
    }
}


db_iterator::db_iterator(db_iterator&& orig)
    : pCursor_(orig.pCursor_)
    , record_(std::move(orig.record_))
{
    orig.pCursor_ = nullptr;
}


db_iterator::~db_iterator() 
{
    if (pCursor_)
    {
        pCursor_->close();
    }
}


db_iterator& db_iterator::operator=(const db_iterator& orig)
{
    if(pCursor_)
    {
        pCursor_->close();
        pCursor_ = nullptr;
    }
    
    if (orig.pCursor_)
    {
        orig.pCursor_->dup(&pCursor_, DB_POSITION);
    }
    
    record_ = orig.record_;
    return *this;
}
    

db_iterator& db_iterator::operator=(db_iterator&& orig)
{
    if(pCursor_)
    {
        pCursor_->close();
        pCursor_ = nullptr;
    }
    
    std::swap(pCursor_, orig.pCursor_);
    record_ = std::move(orig.record_);
    return *this;
}


void db_iterator::increment()
{
    assert(pCursor_);
    
    if (pCursor_)
    {
        RecordID id;
        
        Dbt key;
        Dbt data;
        key.set_flags(DB_DBT_USERMEM);
        key.set_data(id.data());
        key.set_ulen(id.size());

        int err = pCursor_->get(&key, &data, DB_NEXT_NODUP);
        
        if (err == DB_NOTFOUND) // reached end
        {
            pCursor_->close();
            pCursor_ = nullptr;
            record_ = make_Record(NULL_RECORD_ID, RecordData());
        }
        else if (err)
        {
            throw DbException("Failed to advanse cursor", err);
        }
        else
        {
            record_ = make_Record(std::move(id), RecordData(data));
        }
    }
}
   

bool db_iterator::equal(db_iterator const& other) const
{
    return record_.first == other.record_.first;
}


const Record& db_iterator::dereference() const 
{ 
    assert(pCursor_);
    return record_; 
}
