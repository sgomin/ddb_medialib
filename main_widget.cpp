#include "main_widget.hpp"

#include "settings_dlg.hpp"


MainWidget::MainWidget()
{
    Gtk::HBox * pFirstRow = Gtk::manage(new Gtk::HBox());
    Gtk::Button* pSettingsBtn = Gtk::manage(new Gtk::Button("Settings"));
    Gtk::ComboBoxText * pStyleCombo = Gtk::manage(new Gtk::ComboBoxText());
    Gtk::ScrolledWindow * pScrolledWindow = Gtk::manage(new Gtk::ScrolledWindow());
    Gtk::TreeView * pTreeVeiew = Gtk::manage(new Gtk::TreeView());
    
    pSettingsBtn->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onSettings));
    
    pScrolledWindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    pScrolledWindow->add(*pTreeVeiew);
    pStyleCombo->append("By directory structure");
    pStyleCombo->set_active(0);
    pFirstRow->pack_start(*pStyleCombo, Gtk::PACK_EXPAND_WIDGET);
    pFirstRow->pack_end(*pSettingsBtn, Gtk::PACK_SHRINK);
    sidebar_.pack_start(*pFirstRow, Gtk::PACK_SHRINK);
    sidebar_.pack_start(*pScrolledWindow, Gtk::PACK_EXPAND_WIDGET);
    add(sidebar_);
    show_all();
}

void MainWidget::onSettings()
{
    Settings settings = Plugin::getSettings();
    SettingsDlg dlgSettings(settings);
    
    if (dlgSettings.run() == Gtk::RESPONSE_OK)
    {
        Plugin::storeSettings(std::move(settings));
    }
}
