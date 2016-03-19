#include "settings.hpp"

Settings SettingsProvider::getSettings() const
{
	std::lock_guard<std::mutex> lock(mtx_);
    return settings_;
}
	

void SettingsProvider::setSettings(Settings settings)
{
	std::lock_guard<std::mutex> lock(mtx_);
	settings_ = std::move(settings);
}
