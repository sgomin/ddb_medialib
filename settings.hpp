#ifndef SETTINGS_HPP
#define	SETTINGS_HPP

#include <map>
#include <string>

struct Settings
{
    struct Directory
    {
        bool recursive;
    };
 
    typedef std::map<std::string, Directory> Directories;
    Directories directories;
};

#endif	/* SETTINGS_HPP */

