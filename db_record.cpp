#include "db_record.hpp"

#include <boost/functional/hash/hash.hpp>

#include <strstream>

#include <memory.h>

const char FILENAME_DELIMITER = ':';


bool operator==(RecordID const& left, RecordID const& right)
{
	return memcmp(left.data(), right.data(), left.size()) == 0;
}

bool operator!=(RecordID const& left, RecordID const& right)
{
    return !(left == right);
}

size_t hash_value(RecordID const& id)
{
	return boost::hash_range(id.data(), id.data() + id.size());
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

