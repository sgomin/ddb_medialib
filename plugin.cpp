#include "plugin.hpp"

#include "medialib.h"
#include "main_widget.hpp"
#include "database.hpp"
#include "scan_thread.hpp"

#include <sys/types.h>

#include <deadbeef/gtkui_api.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


#include <iostream>
#include <memory.h>

const std::string	CONFIG_FILENAME = "medialib";
const std::string	DB_FILENAME = "medialib.db";


class Plugin::Impl
{
public:
    Impl();
    ~Impl();
    int connect();
	int disconnect();
    
    Settings getSettings() const;
    void     storeSettings(Settings settings);
    
private:
    
    static ddb_gtkui_widget_t * createWidget();
    static void destroyWidget(ddb_gtkui_widget_t *w);
    
	typedef std::unique_ptr<ScanThread> ScanThreadPtr;

#ifdef USE_GTK2
  std::unique_ptr<Gtk::Main>            app_;
#else
    Glib::RefPtr<Gtk::Application>      app_;
#endif
    static ScanEventQueue               eventQueue_;
    static ddb_gtkui_t              *   pGtkUi_;
    const fs::path                      fnSettings_;
	static SettingsProvider             settings_;
	static DbOwnerPtr					db_;
	static ScanThreadPtr				pScanThread_;
    static MainWidget               *   pMainWidget_;
};

ScanEventQueue                  Plugin::Impl::eventQueue_;
ddb_gtkui_t *					Plugin::Impl::pGtkUi_ = nullptr;
SettingsProvider				Plugin::Impl::settings_;
DbOwnerPtr						Plugin::Impl::db_;
Plugin::Impl::ScanThreadPtr		Plugin::Impl::pScanThread_;
MainWidget *                    Plugin::Impl::pMainWidget_ = nullptr;
std::unique_ptr<Plugin::Impl>	Plugin::s_pImpl;

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
        std::clog << "[" PLUGIN_NAME " ] Stopping plugin" << std::endl;
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
Settings Plugin::getSettings()
{
    return s_pImpl->getSettings();
}

// static 
void Plugin::storeSettings(Settings settings)
{
    s_pImpl->storeSettings(std::move(settings));
}


Plugin::Impl::Impl() 
 : fnSettings_(fs::path(deadbeef->get_config_dir()) / CONFIG_FILENAME)
{
	Settings settings;
	settings.load(fnSettings_.string());
	settings_.setSettings(std::move(settings));
}

Plugin::Impl::~Impl()
{
	assert(!pScanThread_);
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
	
	return extensions;
}

}

int Plugin::Impl::connect()
try
{
	const fs::path pathDb = fs::path(deadbeef->get_config_dir()) / DB_FILENAME;
	std::clog << "[" PLUGIN_NAME " ] Opening database at " << pathDb << std::endl;
	db_.reset(new DbOwner(pathDb.string()));
	
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
#ifdef USE_GTK2
	int argc = 0;
	char args[] = "";
	char* argv = args;
	char** pargv = &argv;
	app_.reset( new Gtk::Main(argc, pargv) );
#else
        app_ = Application::create();
#endif
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
    
	std::clog << "[" PLUGIN_NAME " ] Successfully connected" << std::endl;
    return 0;
}
catch(const DbException & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Failed to open database: " 
			<< ex.what() << std::endl;
	return -1;
}
catch(const std::exception & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Failed to connect plugin: " 
			<< ex.what() << std::endl;
	return -1;
}

int Plugin::Impl::disconnect()
try
{
	std::clog << "Stopping scanning thread" << std::endl;
	pScanThread_.reset();
	std::clog << "[" PLUGIN_NAME " ] Closing database " << std::endl;
	db_.reset();
    
    if (pMainWidget_)
    {
        pMainWidget_->onDisconnect();
    }
    
	return 0;
}
catch(const DbException & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Failed to close database: " 
			<< ex.what() << std::endl;
	return -1;
}
catch(const std::exception & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Failed to stop scan thread: " 
			<< ex.what() << std::endl;
	return -1;
}

// static
ddb_gtkui_widget_t * Plugin::Impl::createWidget()
try
{
	std::clog << "[" PLUGIN_NAME " ] Creating widget " << std::endl;
    ddb_gtkui_widget_t *w = 
            static_cast<ddb_gtkui_widget_t*>(malloc(sizeof(ddb_gtkui_widget_t)));
    memset(w, 0, sizeof (*w));
    pMainWidget_ = new MainWidget(
            db_->createReader(), eventQueue_, deadbeef->get_config_dir());
	
	std::clog << "[" PLUGIN_NAME " ] Starting scan thread " << std::endl;
	pScanThread_.reset(new ScanThread(
						settings_, 
						getSupportedExtensions(), 
						std::move(db_),
						eventQueue_,
                        pMainWidget_->getOnChangedDisp()));
	
    w->widget = GTK_WIDGET( pMainWidget_->gobj() );
    w->destroy = &destroyWidget;
	pGtkUi_->w_override_signals (w->widget, w);
    return w;
}
catch(const DbException & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Failed to open database: " 
			<< ex.what() << std::endl;
	return nullptr;
}
catch(const std::exception & ex)
{
	std::cerr << "[" PLUGIN_NAME " ] Initialisation error: " 
			<< ex.what() << std::endl;
	return nullptr;
}

// static 
void Plugin::Impl::destroyWidget(ddb_gtkui_widget_t * w)
{
    std::clog << "[" PLUGIN_NAME " ] Stopping scan thread " << std::endl;
	pScanThread_.reset();
    std::clog << "[" PLUGIN_NAME " ] Destroying widget " << std::endl;
    delete Glib::wrap(w->widget);
    pMainWidget_ = nullptr;
}

Settings Plugin::Impl::getSettings() const
{
	return settings_.getSettings();
}
    
void Plugin::Impl::storeSettings(Settings settings)
{
	settings.save(fnSettings_.string());
	settings_.setSettings(std::move(settings));
	pScanThread_->restart();
}
