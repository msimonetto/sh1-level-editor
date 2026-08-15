#include "core/ChunkManager.h"
#include "core/Config.h"
#include <filesystem>

namespace fs = std::filesystem;

ChunkManager::ChunkManager() {
  m_pipeline.SetLogCallback([this](const std::string &msg, bool isError) {
    this->Log(msg, isError);
  });

  m_pipeline.SetProgressCallback(
      [this](int current, int total, const std::string &op) {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        m_progressCurrent = current;
        m_progressTotal = total;
        m_progressOp = op;
      });

  // Auto-scan on load if path exists
  std::string assetsDir = GetAssetsDir();
  if (!assetsDir.empty() && fs::exists(assetsDir)) {
    ScanAssets();
  }
}

std::string ChunkManager::GetWorkspaceDir() const {
  return (std::filesystem::path(Config::Get().ProjectDirectory) / "workspace")
      .string();
}
std::string ChunkManager::GetAssetsDir() const {
  return (std::filesystem::path(Config::Get().ProjectDirectory) / "assets")
      .string();
}
std::string ChunkManager::GetOverrideDir() const {
  return (std::filesystem::path(Config::Get().GameDirectory) / "pc_port" /
          "build" / "gamedata" / "load")
      .string();
}
std::string ChunkManager::GetBuildDir() const {
  return (std::filesystem::path(Config::Get().GameDirectory) / "pc_port" /
          "build")
      .string();
}
std::string ChunkManager::GetGameBinSource() const {
  return (std::filesystem::path(Config::Get().GameDirectory) / "pc_port" /
          "build" / "gamedata" / "SLUS-00707.bin")
      .string();
}

std::vector<std::string> ChunkManager::ConsumeReloadChunks() {
  std::lock_guard<std::mutex> lock(m_reloadMutex);
  std::vector<std::string> chunks = m_reloadChunks;
  m_reloadChunks.clear();
  return chunks;
}

void ChunkManager::Log(const std::string &msg, bool isError) {
  std::lock_guard<std::mutex> lock(m_consoleMutex);
  std::string prefix = isError ? "[ERROR] " : "";
  m_consoleLines.push_back(prefix + msg);
}

void ChunkManager::ScanAssets() {
  m_parsedChunks.clear();
  m_selectedChunks.clear();

  std::string assetsDir = GetAssetsDir();
  if (assetsDir.empty() || !fs::exists(assetsDir)) {
    Log("Invalid Assets Directory.", true);
    return;
  }

  fs::path searchDir = assetsDir;
  if (fs::exists(searchDir / "BG")) {
    searchDir = searchDir / "BG";
  }

  int count = 0;
  for (const auto &entry : fs::directory_iterator(searchDir)) {
    if (entry.path().extension() == ".IPD" ||
        entry.path().extension() == ".ipd") {
      std::string stem = entry.path().stem().string();
      std::string prefix = stem;
      int x = 0, z = 0;
      bool hasCoords = false;

      if (stem.length() > 4) {
        std::string rest = stem.substr(stem.length() - 4);
        try {
          int x_hex = std::stoi(rest.substr(0, 2), nullptr, 16);
          int z_hex = std::stoi(rest.substr(2, 2), nullptr, 16);

          if (x_hex >= 128)
            x_hex -= 256;
          if (z_hex >= 128)
            z_hex -= 256;

          x = x_hex;
          z = z_hex;
          prefix = stem.substr(0, stem.length() - 4);
          hasCoords = true;
        } catch (...) {
          // Not valid hex
        }
      }

      ChunkInfo info;
      info.name = stem;
      info.prefix = prefix;
      info.x = x;
      info.z = z;
      info.hasCoords = hasCoords;

      m_parsedChunks[prefix].push_back(info);
      count++;
    }
  }

  if (!m_parsedChunks.empty()) {
    // Restore saved prefix if valid, otherwise default to first
    const std::string &saved = Config::Get().SelectedPrefix;
    if (!saved.empty() && m_parsedChunks.find(saved) != m_parsedChunks.end()) {
      m_selectedPrefix = saved;
    } else if (m_selectedPrefix.empty() ||
               m_parsedChunks.find(m_selectedPrefix) == m_parsedChunks.end()) {
      m_selectedPrefix = m_parsedChunks.begin()->first;
    }
  } else {
    m_selectedPrefix = "";
  }

  Log("Scanned " + std::to_string(count) + " chunks across " +
      std::to_string(m_parsedChunks.size()) + " prefixes.");
}
