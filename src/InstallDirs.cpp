#include "InstallDirs.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <wx/filename.h>
#include <wx/log.h>

// FIXME: Missing includes in ocpn_plugin.h
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/menuitem.h>
#include <wx/gdicmn.h>

#include  "ocpn_plugin.h"


typedef std::unordered_map<std::string, std::string> pathmap_t;

static const char SEP  = wxFileName::GetPathSeparator();

std::vector<std::string> split(std::string s, char delimiter)
{
    using namespace std;

    vector<string> tokens;
    size_t start = s.find_first_not_of(delimiter);
    size_t end = start;
    while (start != string::npos) {
        end = s.find(delimiter, start);
        tokens.push_back(s.substr(start, end - start));
        start = s.find_first_not_of(delimiter, end);
    }
    return tokens;
}


inline bool exists(const std::string& name) {
    wxFileName fn(name.c_str());
    if (!fn.IsOk()) {
        return false;
    }
    return fn.FileExists();
}


std::string find_in_path(std::string binary)
{
    using namespace std;

    string path(getenv("PATH"));
    vector<string> elements = split(path, ':');
    for (auto element: elements) {
       string filename = element + "/" + binary;
       if (exists(filename)) {
	   return filename;
       }
    }
    return "";
}

#if 0

static void mkdir(const std::string path)
{
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}



static std::string pluginsConfigDir()
{
    std::string pluginDataDir =
        (*GetpPrivateApplicationDataLocation()).ToStdString();
    pluginDataDir += SEP + "plugins";
    if (!exists(pluginDataDir)) {
        mkdir(pluginDataDir.c_str());
    }
    pluginDataDir += SEP + "install_data";
    if (!exists(pluginDataDir)) {
        mkdir(pluginDataDir);
    }
    return pluginDataDir;
}


static std::string dirListPath(std::string name)
{
    return pluginsConfigDir() + SEP + name + ".dirs";
}


static bool hasDirlist(const std::string name)
{

    std::ifstream s(dirListPath(name));
    if (!s.is_open()) {
        return false;
    }
    s.close();
    return true;
}


static bool parseDirlist(const std::string name, pathmap_t& pathmap)
{
    using namespace std;

    ifstream s(dirListPath(name));
    if (!s.is_open()) {
        return false;
    }
    string line;
    while (getline(s, line)) {
        stringstream lineStream(line);
        string key("");
        string value("undef");
        string token;
        bool ok = true;
        while(lineStream >> token) {
            if      (key == "") key = token;
            else if (token == ":") { value = ""; continue; }
            else if (value == "")  value = token;
            else { wxLogWarning("Cannot parse line: %s", line); ok = false; }
        }
        if (ok && value != "" && key != "") pathmap[key] = value;
std::cout << "setting pathmap[" << key << "] to " << value << endl;
    }
    s.close();
    return true;
}


wxString getInstallationBindir(const char* name)
{
    pathmap_t pathmap;
    if (!parseDirlist(name, pathmap)
        || pathmap.find("bindir") == pathmap.end())
    {
        wxString  s("");
        return s;
    }
    wxString s(pathmap["bindir"]);
    return s;
}

#endif // if 0
