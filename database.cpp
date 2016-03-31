#include "database.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/functional/hash/hash.hpp>

#include <strstream>
#include <sstream>

const char * const DB_MAIN_FILENAME = "main.db";
const char * const DB_FILE_FILENAME = "filename.db";
const char * const DB_PARENID_FILENAME = "parentid.db";
const char FILENAME_DELIMITER = ':';


bool operator==(RecordID const& left, RecordID const& right)
{
	return memcmp(left.data(), right.data(), left.size()) == 0;
}

size_t hash_value(RecordID const& id)
{
	return boost::hash_range(id.data(), id.data() + id.size());
}


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
	
	const u_int32_t envFlags = DB_INIT_CDB | DB_INIT_MPOOL | DB_THREAD | DB_CREATE;
	const u_int32_t dbFlags = DB_THREAD | DB_CREATE;
	
	env_.set_flags(DB_CDB_ALLDB, /*on*/1);
	env_.open(path.c_str(), envFlags, /*mode*/0);
	dbMain_.open(/*txnid*/nullptr, DB_MAIN_FILENAME, 
		/*database*/nullptr, DB_HEAP, dbFlags, /*mode*/0);
	dbFilename_.open(/*txnid*/nullptr, DB_FILE_FILENAME, 
		/*database*/nullptr, DB_BTREE, dbFlags, /*mode*/0);
	dbParentId_.set_flags(DB_DUP | DB_DUPSORT);
	dbParentId_.open(/*txnid*/nullptr, DB_PARENID_FILENAME, 
		/*database*/nullptr, DB_BTREE, dbFlags, /*mode*/0);
	dbMain_.associate(/*txnid*/nullptr, &dbFilename_, &getFileName, /*flags*/0);
	dbMain_.associate(/*txnid*/nullptr, &dbParentId_, &getParentId, /*flags*/0);
}

void Database::close()
{
	dbParentId_.close(0);
	dbFilename_.close(0);
	dbMain_.close(0);
	env_.close(0);
}

RecordData Database::get(const RecordID& id) const
{
	Dbt key(id.data(), id.size());
	Dbt data;
	thread_local static std::vector<char> buffer(256);
		
	while (true)
	{
		try
		{
			data.set_flags(DB_DBT_USERMEM);
			data.set_data(buffer.data());
			data.set_ulen(buffer.size());
			const int err = dbMain_.get(/*txnid*/nullptr, &key, &data, /*flags*/0);
			assert (err == 0);
			break;
		}
		catch(DbException const& ex)
		{
			if (ex.get_errno() !=  DB_BUFFER_SMALL)
			{
				throw;
			}

			buffer.resize(data.get_size() * 1.5);
		}
	}
	
	return RecordData(data);
}

RecordID Database::add(const RecordData& record)
{
	RecordID newId;
	
	Dbt key(newId.data(), 0);
	key.set_flags(DB_DBT_USERMEM);
	key.set_ulen(RecordID::size());
	
	const std::string str = record.data();
	Dbt data(const_cast<char*>(str.c_str()), str.size());
	
	const int err = dbMain_.put(nullptr, &key, &data, DB_APPEND);
	assert (err == 0);
	assert (key.get_size() == RecordID::size());
	return newId;
}

void Database::del(const RecordID& id)
{
	Dbt key(id.data(), id.size());
	// fetch children
	std::vector<RecordID> childrenIds;
	Dbc* pCursor = nullptr;
	dbParentId_.cursor(NULL, &pCursor, 0);
	assert(pCursor);
	
	Dbt keyChild;
	Dbt record;
	record.set_flags(DB_DBT_PARTIAL);
	record.set_doff(0);
	record.set_dlen(0);
	
	int res = pCursor->pget(&key, &keyChild, &record, DB_SET);
	
	while (res == 0)
	{
		childrenIds.push_back(RecordID(keyChild));
		res = pCursor->pget(&key, &keyChild, &record, DB_NEXT_DUP);
	}
	
	if (res != DB_NOTFOUND)
	{
		throw DbException("Failed to obtain children ids", res);
	}
	
	pCursor->close();
	
	// delete children
	for (const RecordID& childId : childrenIds)
	{
		del(childId);
	}
	
	// delete the record itself
	const int err = dbMain_.del(nullptr, &key, /*flags*/0);
	
	if (err)
	{
		std::ostringstream ss;
		ss << "Failed to delete record id='" << id << '\'';
		throw DbException(ss.str().c_str(), err);
	}
}

void Database::replace(const RecordID& id, const RecordData& record)
{
	Dbt key(id.data(), id.size());
	const std::string str = record.data();
	Dbt data(const_cast<char*>(str.c_str()), str.size());
	
	dbMain_.put(nullptr, &key, &data, /*flags*/0);
}

Records Database::children(const RecordID& idParent) const
{
	Records result;
	
	Dbc* pCursor = nullptr;
	dbParentId_.cursor(NULL, &pCursor, 0);
	assert(pCursor);
	
	Dbt keyParent(idParent.data(), idParent.size());
	Dbt keyChild;
	Dbt record;
	
	int res = pCursor->pget(&keyParent, &keyChild, &record, DB_SET);
	
	while (res == 0)
	{
		result.push_back(make_Record(RecordID(keyChild), RecordData(record)));
		res = pCursor->pget(&keyParent, &keyChild, &record, DB_NEXT_DUP);
	}
	
	pCursor->close();
	return result;
}

Record Database::find(const std::string& fileName) const
{
	Dbt key;
	Dbt data;
	
	Dbt fileNameKey(const_cast<char*>(fileName.data()), fileName.size());
	const int err = dbFilename_.pget(nullptr, &fileNameKey, &key, &data, 0);
	
	if (err == DB_NOTFOUND)
	{
		return make_Record(NULL_RECORD_ID, RecordData());
	}
	
	if (err)
	{
		throw DbException("Failed to obtain record by filename key", err);
	}
	
	return make_Record(RecordID(key), RecordData(data));
}

//static 
int Database::getFileName(
	Db */*sdbp*/, const Dbt */*pkey*/, const Dbt *pdata, Dbt *skey)
{
	assert(pdata);
	assert(skey);
	
	const char * const pData = static_cast<const char*>(pdata->get_data());
	const char * const pDataEnd = pData + pdata->get_size();
	
	int cnt = 0;
	const char * pFilename = std::find_if(pData, pDataEnd, [&cnt](char c)->bool
	{
		return c == ' ' && ++cnt == 4;
	});
	
	if (pFilename == pDataEnd || pFilename + 1 == pDataEnd)
	{
		return -1;
	}
	
	++pFilename;
	
	const char * const pFilenameEnd = std::find(pFilename, pDataEnd, FILENAME_DELIMITER);
	
	if (pFilenameEnd == pDataEnd)
	{
		return -1;
	}
	
	const size_t fileNameSize = pFilenameEnd - pFilename;
	
	skey->set_data(const_cast<char*>(pFilename));
    skey->set_size(fileNameSize);
	
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
	
	RecordID  parentID;
	strm >> parentID;
	
	void * pMem = malloc(parentID.size());
	memcpy(pMem, parentID.data(), parentID.size());
	
	skey->set_data(pMem);
    skey->set_size(parentID.size());
	skey->set_flags(DB_DBT_APPMALLOC);
	
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

std::istream& operator>> (std::istream& strm, RecordData::Header& header)
{
	char delim;
	int isDir;
	strm >> header.parentID >> header.lastWriteTime >> isDir >> std::ws;
	header.isDir = !!isDir;
	return std::getline(strm, header.fileName, FILENAME_DELIMITER).get(delim);
}

std::ostream& operator<< (std::ostream& strm, const RecordData::Header& header)
{
	return strm << header.parentID << ' ' 
				<< header.lastWriteTime << ' '
				<< (header.isDir ? 1 : 0) << ' '
				<< header.fileName << FILENAME_DELIMITER;
}


// ----- RecordID --------------------------------------------------------------

RecordID::RecordID(const Dbt& dbRec)
{
	assert(dbRec.get_size() == size());
	memcpy(data_.data(), dbRec.get_data(), dbRec.get_size());
}

// static 
RecordID RecordID::nil()
{
	RecordID nilId;
	nilId.data_.fill(0);
	return nilId;
}


// ----- RecordData ------------------------------------------------------------

RecordData::RecordData() :
	header()
{
}
    
RecordData::RecordData(const Dbt& dbRec)
{
	std::istrstream strm(
		static_cast<const char*>(dbRec.get_data()), dbRec.get_size());

	strm >> header;
}

RecordData::RecordData(const RecordID& parentID, 
					   time_t lastWriteTime,
					   bool isDir,
					   const std::string& fileName)
{
	header.parentID = parentID;
	header.lastWriteTime = lastWriteTime;
	header.isDir = isDir;
	header.fileName = fileName;
}

std::string RecordData::data() const
{
	std::ostringstream strm;
	
	strm << header;
	return strm.str();
}