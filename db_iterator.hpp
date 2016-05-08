#ifndef DB_ITERATOR_HPP
#define DB_ITERATOR_HPP

#include "db_record.hpp"

#include <boost/iterator/iterator_facade.hpp>

class Dbc;

class db_iterator 
    :  public boost::iterator_facade<
        db_iterator
      , Record const
      , boost::forward_traversal_tag>
{
public:
    explicit db_iterator(Dbc* pCursor);
    db_iterator(const db_iterator& orig);
    db_iterator(db_iterator&& orig);
    
    virtual ~db_iterator();
    
    db_iterator& operator=(const db_iterator& orig);
    db_iterator& operator=(db_iterator&& orig);
    
private:
    friend class boost::iterator_core_access;

    void increment();
    bool equal(db_iterator const& other) const;
    const Record& dereference() const { return record_; }
    
    Dbc* pCursor_;
    Record record_;
};

#endif /* DB_ITERATOR_HPP */

