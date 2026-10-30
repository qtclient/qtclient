#include "config.h"

#include "resourcemanager.h"

Config::Config() = default;

bool Config::load(const std::string& file)
{
    // Engine settings used to be an OTML document. The product reads config.ini
    // through ConfigManager; this object only keeps values set in-process.
    m_fileName = file;
    return tibia::files::data().fileExists(file);
}

bool Config::unload()
{
    if (!isLoaded())
        return false;
    m_values.clear();
    m_lists.clear();
    m_fileName.clear();
    return true;
}

bool Config::save()
{
    return !m_fileName.empty();
}

void Config::clear()
{
    m_values.clear();
    m_lists.clear();
}

void Config::setValue(const std::string& key, const std::string& value)
{
    if (value.empty()) {
        remove(key);
        return;
    }
    m_values[key] = value;
}

void Config::setList(const std::string& key, const std::vector<std::string>& list)
{
    if (list.empty()) {
        m_lists.erase(key);
        return;
    }
    m_lists[key] = list;
}

bool Config::exists(const std::string& key)
{
    return m_values.find(key) != m_values.end() || m_lists.find(key) != m_lists.end();
}

std::string Config::getValue(const std::string& key)
{
    const auto it = m_values.find(key);
    return it == m_values.end() ? std::string{} : it->second;
}

std::vector<std::string> Config::getList(const std::string& key)
{
    const auto it = m_lists.find(key);
    return it == m_lists.end() ? std::vector<std::string>{} : it->second;
}

void Config::remove(const std::string& key)
{
    m_values.erase(key);
    m_lists.erase(key);
}

bool Config::isLoaded() const
{
    return !m_fileName.empty();
}

std::string Config::getFileName()
{
    return m_fileName;
}
