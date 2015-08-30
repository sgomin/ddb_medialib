#include "database.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
//#include <boost/archive/binary_iarchive.hpp>
//#include <boost/archive/binary_oarchive.hpp>

#include <strstream>
#include <sstream>

const char * const DB_MAIN_FILENAME = "main.db";
const char * const DB_FILE_FILENAME = "filename.db";
const char * const DB_PARENID_FILENAME = "parentid.db";

//namespace boost {
//namespace serialization {

//template<class Archive>
//void serialize(Archive & ar, Record & r, const unsigned int /*version*/)
//{
//	ar & r.parentID;
//	ar & r.lastWriteTime;
//    ar & r.fileName;
//}

//} // namespace serialization
//} // namespace boost

Database::Database() 
 : env_(0)
 , dbMain_(&env_, 0)
 , dbFilename_(&env_, 0)
 , dbParentId_(&env_, 0)
{
	static_assert(std::is_base_of<std::exception, DbException>::value, 
			"DbException isn't std::exception");
}

void Database::open(const std::string& path)
{
	fs::create_directory(path);
	
	const u_int32_t envFlags = DB_INIT_CDB | DB_INIT_MPOOL | DB_CREATE;
	
	env_.open(path.c_str(), envFlags, /*mode*/0);
	dbMain_.open(/*txnid*/nullptr, DB_MAIN_FILENAME, 
		/*database*/nullptr, DB_HEAP, DB_CREATE, /*mode*/0);
	dbFilename_.open(/*txnid*/nullptr, DB_FILE_FILENAME, 
		/*database*/nullptr, DB_BTREE, DB_CREATE, /*mode*/0);
	dbParentId_.set_flags(DB_DUPSORT);
	dbParentId_.open(/*txnid*/nullptr, DB_PARENID_FILENAME, 
		/*database*/nullptr, DB_BTREE, DB_CREATE, /*mode*/0);
	dbMain_.associate(/*txnid*/nullptr, &dbFilename_, &getFileName, /*flags*/0);
	dbMain_.associate(/*txnid*/nullptr, &dbParentId_, &getParentId, /*flags*/0);
}

Record Database::get(const RecordID& id) const
{
	Dbt key(id.data(), id.size());
	Dbt data;
	
	const int err = dbMain_.get(/*txnid*/nullptr, &key, &data, /*flags*/0);
	assert (err == 0);
	
	return Record(data);
}

RecordID Database::add(const Record& record)
{
	Dbt key;
	const std::string str = record.data();
	Dbt data(const_cast<char*>(str.c_str()), str.size());
	
	const int err = dbMain_.put(nullptr, &key, &data, DB_APPEND);
	assert (err == 0);
	
	return RecordID(key);
}

void Database::replace(const RecordID& id, const Record& record)
{
	Dbt key(id.data(), id.size());
	const std::string str = record.data();
	Dbt data(const_cast<char*>(str.c_str()), str.size());
	
	dbMain_.put(nullptr, &key, &data, DB_APPEND);
}

IDRecordPairs Database::children(const RecordID& idParent) const
{
	IDRecordPairs result;
	
	Dbc* pCursor = nullptr;
	dbParentId_.cursor(NULL, &pCursor, 0);
	assert(pCursor);
	
	Dbt keyParent(idParent.data(), idParent.size());
	Dbt keyChild;
	Dbt record;
	
	int res = pCursor->pget(&keyParent, &keyChild, &record, DB_SET);
	
	while (res == 0)
	{
		result.push_back(std::make_pair(RecordID(keyChild), Record(record)));
		res = pCursor->pget(&keyParent, &keyChild, &record, DB_NEXT_DUP);
	}
	
	pCursor->close();
	return result;
}

boost::optional<IDRecordPair> Database::find(const std::string& fileName) const
{
	Dbt key;
	Dbt data;
	
	Dbt fileNameKey(const_cast<char*>(fileName.data()), fileName.size());
	const int err = dbFilename_.pget(nullptr, &fileNameKey, &key, &data, 0);
	
	if (err == DB_NOTFOUND)
	{
		return boost::optional<IDRecordPair>();
	}
	
	if (err)
	{
		throw DbException("Failed to obtain record by filename key", err);
	}
	
	return std::make_pair(RecordID(key), Record(data));
}

//static 
int Database::getFileName(
	Db */*sdbp*/, const Dbt */*pkey*/, const Dbt *pdata, Dbt *skey)
{
	assert(pdata);
	assert(skey);
	
	std::istrstream strm(
		static_cast<const char*>(pdata->get_data()), pdata->get_size());

	Record::Header header;
	strm >> header;
	
	skey->set_data(const_cast<char*>(header.fileName.data()));
    skey->set_size(header.fileName.size());
	
	return 0;
}

//static 
int Database::getParentId(
        Db* /*sdbp*/, const Dbt* /*pkey*/, const Dbt* pdata, Dbt* skey)
{
	assert(pdata);
	assert(skey);
	
	std::istrstream strm(
		static_cast<const char*>(pdata->get_data()), pdata->get_size());

	RecordID parentID;
	strm >> parentID;
	
	skey->set_data(parentID.data());
    skey->set_size(parentID.size());
	
	return 0;
}

std::istream& operator>> (std::istream& strm, RecordID& recordID)
{
	return strm >> *reinterpret_cast<db_pgno_t*>(recordID.data())
		>> *reinterpret_cast<db_indx_t*>(recordID.data() + sizeof(db_pgno_t));
}

std::ostream& operator<< (std::ostream& strm, const RecordID& recordID)
{
	return strm << *reinterpret_cast<const db_pgno_t*>(recordID.data()) << ' '
		<< *reinterpret_cast<const db_indx_t*>(recordID.data() + sizeof(db_pgno_t));
}

const char FILENAME_DELIMITER = ':';

std::istream& operator>> (std::istream& strm, Record::Header& header)
{
	char delim;
	strm >> header.parentID >> header.lastWriteTime >> std::ws;
	return std::getline(strm, header.fileName, FILENAME_DELIMITER).get(delim);
}

std::ostream& operator<< (std::ostream& strm, const Record::Header& header)
{
	return strm << header.parentID << ' ' 
				<< header.lastWriteTime << ' ' 
				<< header.fileName << FILENAME_DELIMITER;
}

RecordID::RecordID()
{
	data_.fill(0);
}

RecordID::RecordID(const Dbt& dbRec)
{
	assert(dbRec.get_size() == size());
	memcpy(data_.data(), dbRec.get_data(), dbRec.get_size());
}

Record::Record() :
	header()
{
}
    
Record::Record(const Dbt& dbRec)
{
	std::istrstream strm(
		static_cast<const char*>(dbRec.get_data()), dbRec.get_size());

	strm >> header;
}

std::string Record::data() const
{
	std::ostringstream strm;
	
	strm << header;
	return strm.str();
}