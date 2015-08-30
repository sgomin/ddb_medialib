#ifndef MAIN_WIDGET_HPP
#define	MAIN_WIDGET_HPP

#include "plugin.h"

#include <gtkmm.h>

class MainWidget : public Gtk::EventBox
{
public:
    MainWidget();
    
private:
    void onSettings();
    
    Gtk::VBox   sidebar_;
};


#endif	/* MAIN_WIDGET_HPP */

