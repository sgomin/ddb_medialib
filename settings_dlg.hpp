#ifndef SETTINGS_DLG_HPP
#define	SETTINGS_DLG_HPP

#include "settings.hpp"

#include <gtkmm.h>

class SettingsDlg : public Gtk::Dialog
{
public:
    SettingsDlg(Settings & settings);
    
private:
    void onAddDir();
    void onDelDir();
    void onSelChanged(Gtk::ListBoxRow * pRow);
    
    void addDirectory(
            const std::string & dirname, 
            const Settings::Directory & settings);
    
    Settings              & settings_;
    Gtk::ListBox            lbDirectories_;
    Gtk::Button             btnDel_;
};

#endif	/* SETTINGS_DLG_HPP */

