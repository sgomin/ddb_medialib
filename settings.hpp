#ifndef SETTINGS_HPP
#define	SETTINGS_HPP

#include <map>
#include <string>
#include <mutex>

struct Settings
{
    struct Directory
    {
        bool recursive;
    };
 
    typedef std::map<std::string, Directory> Directories;
    Directories directories;
};


class SettingsProvider
{
public:
	Settings getSettings() const;
	void setSettings(Settings settings);
	
private:
	mutable std::mutex	mtx_;
	Settings			settings_;
};

#endif	/* SETTINGS_HPP */

