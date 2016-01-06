#include "medialib.h"
#include "plugin.hpp"

#include <iostream>

#define DB_API_VERSION_MAJOR 1
#define DB_API_VERSION_MINOR 7

#define PLUGIN_VERSION_MAJOR    0
#define PLUGIN_VERSION_MINOR    1

DB_functions_t * deadbeef = nullptr;

static const char settings_dlg[] = 
    "property \"Enable plugin\"                 checkbox "              CONFSTR_MEDIALIB_ENABLED              " 1 ;\n"
    "property \"Default path: \"                file "                 CONFSTR_MEDIALIB_ROOTPATH         " \" ~\\home\\Music \" ;\n";

extern "C" DB_plugin_t * ddb_misc_medialib_load (DB_functions_t *ddb)
{
    std::clog << "Loading " PLUGIN_NAME << std::endl;
    
    deadbeef = ddb;
        
    if (!deadbeef)
    {
        std::cerr << "Invalid parameter!" << std::endl;
        return nullptr;
    }
    
    static DB_plugin_t plugin;
    plugin.api_vmajor = DB_API_VERSION_MAJOR;
    plugin.api_vminor = DB_API_VERSION_MINOR;
    plugin.type            = DB_PLUGIN_MISC;
    plugin.version_major   = PLUGIN_VERSION_MAJOR;
    plugin.version_minor   = PLUGIN_VERSION_MINOR;
    //#if GTK_CHECK_VERSION(3,0,0)
    //    .plugin.id              = "filebrowser-gtk3",
    //#else
    plugin.id              = "madialib";
    //#endif
    plugin.name            = PLUGIN_NAME;
    plugin.descr           = "Media library";
    plugin.copyright       = "TODO";
    plugin.website         = "http://TODO";
    plugin.start           = &Plugin::start;
    plugin.stop            = &Plugin::stop;
    plugin.connect         = &Plugin::connect;
    plugin.disconnect      = &Plugin::disconnect;
    plugin.configdialog    = settings_dlg;
    
    return &plugin;
}
