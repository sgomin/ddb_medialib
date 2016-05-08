#ifndef DATABASE_HPP
#define	DATABASE_HPP

#include "db_record.hpp"
#include "db_iterator.hpp"

#include <vector>

#include <db_cxx.h>


typedef std::vector<Record> Records;

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
//    Record      find(const std::string& fileName) const;
    
    Record      firstDir() const;
    Record      nextDir(const RecordID& curr) const;
    
    db_iterator dirs_begin() const;
    db_iterator dirs_end() const;
    
private:
//    static int getFileName(
//        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    static int getParentId(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    static int getDirId(
        Db* sdbp, const Dbt* pkey, const Dbt* pdata, Dbt* skey);
    
    static std::string convert(const RecordData& record);

private:
    DbEnv       env_;
	mutable Db	dbMain_;
//    mutable Db  dbFilename_;
    mutable Db  dbParentId_;
    mutable Db  dbDirs_;
};

#endif	/* DATABASE_HPP */

