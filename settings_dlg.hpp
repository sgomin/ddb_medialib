#ifndef SETTINGS_DLG_HPP
#define	SETTINGS_DLG_HPP

#include "settings.hpp"

#include <gtkmm.h>

class SettingsDlg : public Gtk::Dialog
{
public:
    SettingsDlg(Settings & settings);
    
private:
    void initDirList();
    void onAddDir();
    void onDelDir();
//    bool onSelNotify(GdkEventSelection* selection_event);
    
    void addDirectory(
            const std::string & dirname, 
            const Settings::Directory & settings);
    
    Settings                   & settings_;
    Gtk::TreeView		 dirList_;
    Glib::RefPtr<Gtk::ListStore> pListModel_;
    Gtk::Button                  btnDel_;
};

#endif	/* SETTINGS_DLG_HPP */

