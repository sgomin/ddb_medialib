#ifndef DB_ITERATOR_HPP
#define DB_ITERATOR_HPP

#include "db_record.hpp"

#include <boost/iterator/iterator_facade.hpp>
#include <boost/range/iterator_range.hpp>

struct sqlite3_stmt;

class db_file_iterator 
    :  public boost::iterator_facade<
        db_file_iterator
      , FileRecord const
      , boost::single_pass_traversal_tag>
{
public:
    explicit db_file_iterator(struct sqlite3_stmt* pCursor);
    ~db_file_iterator();
    
private:
    friend class boost::iterator_core_access;

    void increment();
    bool equal(db_file_iterator const& other) const;
    FileRecord const& dereference() const;
    
    void readNextRecord();
    
    struct sqlite3_stmt* pCursor_;
    FileRecord           rec_;
};

using db_file_iterator_range = boost::iterator_range<db_file_iterator>;

#endif /* DB_ITERATOR_HPP */

