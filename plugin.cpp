#include "plugin.h"

#include "medialib.h"
#include "main_widget.hpp"
#include "database.hpp"
#include "scan_thread.hpp"

#include <sys/types.h>

#include <deadbeef/gtkui_api.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <memory.h>

const std::string	CONFIG_FILENAME = "medialib";
const std::string	CONFIG_DIRECTORIES_KEY = "directories";
const std::string	CONFIG_RECURSIVE_KEY = "recursive";
const std::string	DB_DIR = "medialibdb";


class Plugin::Impl
{
public:
    Impl();
    ~Impl();
    int connect();
	int disconnect();
    
    Settings const & getSettings() const;
    void     storeSettings(Settings settings);
    
private:
    
    static ddb_gtkui_widget_t * createWidget();
    static void destroyWidget(ddb_gtkui_widget_t *w);
    
    Glib::RefPtr<Gtk::Application>      app_;
    ddb_gtkui_t                     *   pGtkUi_;
    const fs::path                      fnSettings_;
    Settings                            settings_;
	static Database						db_;
	Extensions							extensions_;
	std::unique_ptr<ScanThread>			pScanThread_;
};

Database Plugin::Impl::db_;
std::unique_ptr<Plugin::Impl> Plugin::s_pImpl;

//static 
int Plugin::start()
{
    try
    {
        std::clog << "[" PLUGIN_NAME " ] Starting plugin" << std::endl;
        s_pImpl.reset(new Impl());
        return 0;
    }
    catch(const std::exception & ex)
    {
        std::cerr << "[" PLUGIN_NAME " ] Failed to start plugin: " 
                << ex.what() << std::endl;
        return -1;
    }
}

//static 
int Plugin::stop()
{
    try
    {
        std::clog << "[" PLUGIN_NAME " ] Stoping plugin" << std::endl;
        s_pImpl.reset();
        return 0;
    }
    catch(const std::exception & ex)
    {
        std::cerr << "[" PLUGIN_NAME " ] Failed to stop plugin: " 
                << ex.what() << std::endl;
        return -1;
    }
}

//static 
int Plugin::connect()
{
    std::clog << "[" PLUGIN_NAME " ] Connecting plugin" << std::endl;
    return s_pImpl->connect();
}

//static 
int Plugin::disconnect ()
{
    std::clog << "[" PLUGIN_NAME " ] Disconnecting plugin" << std::endl;
	return s_pImpl->disconnect();
}

// static 
Settings const & Plugin::getSettings()
{
    return s_pImpl->getSettings();
}

// static 
void Plugin::storeSettings(Settings settings)
{
    s_pImpl->storeSettings(std::move(settings));
}


Plugin::Impl::Impl() 
 : app_(Gtk::Application::create())
 , fnSettings_(fs::path(deadbeef->get_config_dir()) / CONFIG_FILENAME)
{
	using boost::property_tree::ptree;
	using namespace boost::property_tree::json_parser;
	
	ptree tree;
	read_json(fnSettings_.string(), tree);
	ptree::assoc_iterator itDirs = tree.find(CONFIG_DIRECTORIES_KEY);

	if (itDirs != tree.not_found())
	{
		for (auto dir : itDirs->second)
		{
			Settings::Directory dirSettings;
			dirSettings.recursive = dir.second.get(CONFIG_RECURSIVE_KEY, true);
			settings_.directories[dir.first] = std::move(dirSettings);
		}
	}
	
	const fs::path pathDb = fs::path(deadbeef->get_config_dir()) / DB_DIR;
	db_.open(pathDb.string());
}

Plugin::Impl::~Impl()
{
	try
    {
		db_.close();
		storeSettings(std::move(settings_));
    }
	catch(const DbException & ex)
    {
        std::cerr << "[" PLUGIN_NAME " ] Failed to close database: " 
                << ex.what() << std::endl;
    }
    catch(const std::exception & ex)
    {
        std::cerr << "[" PLUGIN_NAME " ] Failed to save plugin settings: " 
                << ex.what() << std::endl;
    }
}


namespace {
	
Extensions getSupportedExtensions()
{
	Extensions extensions;
	struct DB_decoder_s **decoders = deadbeef->plug_get_decoder_list();
    
	for (size_t i = 0; decoders[i]; i++) 
	{
        const gchar **exts = decoders[i]->exts;
        
		for (size_t j = 0; exts[j]; j++)
		{
			std::string ext = std::string(".") + exts[j];
            extensions.insert(std::move(ext));
		}
    }
	
	extensions.insert(".flac");
	extensions.insert(".wv");
	extensions.insert(".mp3");
	
	return extensions;
}

}

int Plugin::Impl::connect()
{
    pGtkUi_ = (ddb_gtkui_t *) deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);
    
    if (pGtkUi_) 
    {
        std::clog << "[" PLUGIN_NAME " ] Found '" DDB_GTKUI_PLUGIN_ID "' plugin " 
                << pGtkUi_->gui.plugin.version_major << "." 
                << pGtkUi_->gui.plugin.version_minor << std::endl;
        
        if (pGtkUi_->gui.plugin.version_major < 2) 
        {
            std::cerr << "[" PLUGIN_NAME " ] Error: incompatible version of '" 
                    << DDB_GTKUI_PLUGIN_ID << "' plugin!" <<std::endl;
            return -1;
        }
        
        pGtkUi_->w_reg_widget(
            PLUGIN_NAME, DDB_WF_SINGLE_INSTANCE, &createWidget, "medialib", NULL);
    }
    else
    {
        std::cerr << "[" PLUGIN_NAME " ] Error: could not find '" 
                DDB_GTKUI_PLUGIN_ID "' plugin (gtkui api version " 
                << DDB_GTKUI_API_VERSION_MAJOR 
                << "." << DDB_GTKUI_API_VERSION_MINOR << ")!" << std::endl;
        return -1;
    }
    
	
	extensions_ = getSupportedExtensions();
	pScanThread_.reset(new ScanThread(settings_.directories, extensions_, db_));
	
    std::clog << "[" PLUGIN_NAME " ] Successfully connected" << std::endl;
    return 0;
}

int Plugin::Impl::disconnect()
{
	pScanThread_.reset();
	return 0;
}

// static
ddb_gtkui_widget_t * Plugin::Impl::createWidget()
{
    std::clog << "[" PLUGIN_NAME " ] Creating widget " << std::endl;
    ddb_gtkui_widget_t *w = 
            static_cast<ddb_gtkui_widget_t*>(malloc(sizeof(ddb_gtkui_widget_t)));
    memset(w, 0, sizeof (*w));
    MainWidget * pMainWidget = new MainWidget(db_);
    w->widget = GTK_WIDGET( pMainWidget->gobj() );
    w->destroy = &destroyWidget;
    return w;
}

// static 
void Plugin::Impl::destroyWidget(ddb_gtkui_widget_t * w)
{
    std::clog << "[" PLUGIN_NAME " ] Destroying widget " << std::endl;
    delete Glib::wrap(w->widget);
}

Settings const & Plugin::Impl::getSettings() const
{
    return settings_;
}
    
void Plugin::Impl::storeSettings(Settings settings)
{
	using boost::property_tree::ptree;
	using namespace boost::property_tree::json_parser;
	
    settings_ = std::move(settings);
	ptree treeMain;
	ptree::iterator itDirs = treeMain.push_back(
			std::make_pair(CONFIG_DIRECTORIES_KEY, ptree()));
	
	for (auto dir : settings_.directories)
	{
		ptree::iterator itDir = itDirs->second.push_back(
				std::make_pair(dir.first, ptree()));
		
		itDir->second.put(CONFIG_RECURSIVE_KEY, dir.second.recursive);
	}
	
    write_json(fnSettings_.string(), treeMain);
}
