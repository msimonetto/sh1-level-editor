#pragma once

#include <string>
#include <map>
#include <filesystem>

class Dictionary {
public:
    Dictionary();
    ~Dictionary() = default;

    void Load();
    void Save();

    // Map prefix abbreviations (e.g., "THR") to their full names (e.g., "Old Silent Hill (Normal)")
    std::map<std::string, std::string> PrefixNames;

    // Map chunk names to user-defined aliases
    std::map<std::string, std::string> ChunkAliases;

    // Map other types in the future (e.g., ObjectAliases, TextureAliases)
    // std::map<std::string, std::string> ObjectAliases;
    // std::map<std::string, std::string> TextureAliases;

private:
    std::string m_dictPath;
    std::string m_templatePath;
    void ParseJsonFile(const std::string& path);
};

