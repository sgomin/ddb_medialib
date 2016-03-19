#ifndef PLUGIN_H
#define	PLUGIN_H

#include "settings.hpp"

#include <memory>

class Plugin
{
public:
    static int start();
    static int stop();
    static int connect();
    static int disconnect();
    
    static Settings getSettings();
    static void     storeSettings(Settings settings);
    
private:
    class Impl;
    
    static std::unique_ptr<Impl> s_pImpl;
};

#endif	/* PLUGIN_H */

