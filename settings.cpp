#include "settings.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


const std::string	CONFIG_DIRECTORIES_KEY = "directories";
const std::string	CONFIG_RECURSIVE_KEY = "recursive";


Settings SettingsProvider::getSettings() const
{
	std::lock_guard<std::mutex> lock(mtx_);
    return settings_;
}
	

void SettingsProvider::setSettings(Settings settings)
{
	std::lock_guard<std::mutex> lock(mtx_);
	settings_ = std::move(settings);
}


void Settings::load(std::string const& fileName)
{
    using boost::property_tree::ptree;
	using namespace boost::property_tree::json_parser;
    
    ptree tree;
	read_json(fileName, tree);
	ptree::assoc_iterator itDirs = tree.find(CONFIG_DIRECTORIES_KEY);

	if (itDirs != tree.not_found())
	{
		for (auto dir : itDirs->second)
		{
			Settings::Directory dirSettings;
			dirSettings.recursive = dir.second.get(CONFIG_RECURSIVE_KEY, true);
			directories[dir.first] = std::move(dirSettings);
		}
	}
}
    

void Settings::save(std::string const& fileName)
{
    using boost::property_tree::ptree;
	using namespace boost::property_tree::json_parser;
	
	ptree treeMain;
	ptree::iterator itDirs = treeMain.push_back(
			std::make_pair(CONFIG_DIRECTORIES_KEY, ptree()));
	
	for (auto dir : directories)
	{
		ptree::iterator itDir = itDirs->second.push_back(
				std::make_pair(dir.first, ptree()));
		
		itDir->second.put(CONFIG_RECURSIVE_KEY, dir.second.recursive);
	}
	
    write_json(fileName, treeMain);
}
    