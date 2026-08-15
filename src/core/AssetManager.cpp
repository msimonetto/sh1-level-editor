#include "core/AssetManager.h"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

static fs::path GetScriptPath(const std::string &scriptName) {
  fs::path cwd = fs::current_path();
  // If run from repo root
  fs::path p1 = cwd / "tooling" / "unified_cpp_editor" / "scripts" / scriptName;
  if (fs::exists(p1))
    return p1;
  // If run from tooling/unified_cpp_editor/build
  fs::path p2 = cwd / ".." / "scripts" / scriptName;
  if (fs::exists(p2))
    return p2;
  // Fallback
  return p2;
}

bool AssetManager::ExtractToWorkspace(const std::vector<std::string> &chunks,
                                     const std::string &completeDir,
                                     const std::string &workspaceDir,
                                     const std::string &projectDir) {
  Log("[EXTRACT] Starting native ExtractToWorkspace via Python...");

  if (chunks.empty()) {
    Log("[EXTRACT] No chunks selected.");
    return false;
  }

  fs::path origDir = fs::path(workspaceDir);
  fs::create_directories(origDir);

  fs::path scriptPath = GetScriptPath("backend/chunk_extractor.py");

  bool allSuccess = true;
  for (size_t i = 0; i < chunks.size(); i++) {
    std::string cmd = "python \"" + scriptPath.string() + "\"";
    cmd += " --complete-dir \"" + completeDir + "\"";
    cmd += " --out-dir \"" + workspaceDir + "\"";
    cmd += " \"" + chunks[i] + "\"";

    int res = std::system(cmd.c_str());
    if (res != 0) {
      Log("[EXTRACT] chunk_extractor.py failed for " + chunks[i], true);
      allSuccess = false;
    }

    if (m_progressCallback) {
      m_progressCallback((int)i + 1, (int)chunks.size(),
                         "Extracting " + chunks[i]);
    }
  }

  Log("[EXTRACT] ExtractToWorkspace completed.");
  return allSuccess;
}

bool AssetManager::DeployToTarget(const std::vector<std::string> &chunks,
                                 const std::string &workspaceDir,
                                 const std::string &overrideDir,
                                 const std::string &projectDir) {
  Log("[DEPLOY] Starting smart deploy via Python...");

  if (chunks.empty()) {
    Log("[DEPLOY] No chunks selected.");
    return false;
  }

  fs::path scriptPath = GetScriptPath("backend/deploy_workspace.py");
  fs::path assetsDir = fs::path(projectDir) / "data" / "assets";

  bool allSuccess = true;
  for (size_t i = 0; i < chunks.size(); i++) {
    std::string cmd = "python \"" + scriptPath.string() + "\"";
    cmd += " --workspace-dir \"" + workspaceDir + "\"";
    cmd += " --assets-dir \"" + assetsDir.string() + "\"";
    cmd += " --override-dir \"" + overrideDir + "\"";
    cmd += " \"" + chunks[i] + "\"";

    int res = std::system(cmd.c_str());
    if (res != 0) {
      Log("[DEPLOY] deploy_workspace.py failed for " + chunks[i], true);
      allSuccess = false;
    }

    if (m_progressCallback) {
      m_progressCallback((int)i + 1, (int)chunks.size(),
                         "Deploying " + chunks[i]);
    }
  }

  Log("[DEPLOY] DeployToTarget completed.");
  return allSuccess;
}

bool AssetManager::DeleteSelected(const std::string &targetType,
                                 const std::vector<std::string> &chunks,
                                 bool deleteTextures,
                                 const std::string &workspaceDir,
                                 const std::string &overrideDir,
                                 const std::string &projectDir) {
  Log("[DELETE] Starting targeted deletion from " + targetType + "...");
  if (chunks.empty()) {
    Log("[DELETE] No chunks selected.", true);
    return false;
  }

  fs::path scriptPath = GetScriptPath("backend/manage_workspace.py");
  bool allSuccess = true;
  for (size_t i = 0; i < chunks.size(); i++) {
    std::string cmd = "python \"" + scriptPath.string() + "\" delete_selected";
    cmd += " --target " + targetType;
    if (deleteTextures)
      cmd += " --delete-textures";
    cmd += " --workspace-dir \"" + workspaceDir + "\"";
    cmd += " --override-dir \"" + overrideDir + "\"";
    cmd += " --chunks \"" + chunks[i] + "\"";

    int res = std::system(cmd.c_str());
    if (res != 0) {
      Log("[DELETE] manage_workspace.py failed for " + chunks[i], true);
      allSuccess = false;
    }

    if (m_progressCallback) {
      m_progressCallback((int)i + 1, (int)chunks.size(),
                         "Deleting " + chunks[i]);
    }
  }

  Log("[DELETE] DeleteSelected completed.");
  return allSuccess;
}

bool AssetManager::ClearEntire(const std::string &targetType,
                              const std::string &workspaceDir,
                              const std::string &overrideDir,
                              const std::string &projectDir) {
  Log("[CLEAR] Clearing entire " + targetType + "...");

  fs::path scriptPath = GetScriptPath("backend/manage_workspace.py");
  std::string cmd = "python \"" + scriptPath.string() + "\" clear_all";
  cmd += " --target " + targetType;
  cmd += " --workspace-dir \"" + workspaceDir + "\"";
  cmd += " --override-dir \"" + overrideDir + "\"";

  Log("[CLEAR] Executing clear script...");
  int res = std::system(cmd.c_str());
  if (res != 0) {
    Log("[CLEAR] Clear script failed.", true);
    return false;
  }
  Log("[CLEAR] Cleared " + targetType + " successfully.");
  return true;
}

bool AssetManager::RevertSelected(const std::vector<std::string> &chunks,
                                 bool revertDependencies,
                                 const std::string &workspaceDir,
                                 const std::string &assetsDir) {
  Log("[REVERT] Starting revert to original...");
  if (chunks.empty()) {
    Log("[REVERT] No chunks selected.", true);
    return false;
  }

  fs::path scriptPath = GetScriptPath("backend/chunk_extractor.py");
  bool allSuccess = true;
  for (size_t i = 0; i < chunks.size(); i++) {
    std::string cmd = "python \"" + scriptPath.string() + "\"";
    cmd += " --complete-dir \"" + assetsDir + "\"";
    cmd += " --out-dir \"" + workspaceDir + "\"";
    if (!revertDependencies)
      cmd += " --skip-dependencies";
    cmd += " \"" + chunks[i] + "\"";

    int res = std::system(cmd.c_str());
    if (res != 0) {
      Log("[REVERT] chunk_extractor.py failed for " + chunks[i], true);
      allSuccess = false;
    }

    if (m_progressCallback) {
      m_progressCallback((int)i + 1, (int)chunks.size(),
                         "Reverting " + chunks[i]);
    }
  }

  Log("[REVERT] RevertSelected completed.");
  return allSuccess;
}

bool AssetManager::DeployOverlayToDecomp(const std::string &mapKey) {
  Log("[DEPLOY OVERLAY] Packing " + mapKey + " to decomp C source...");
  fs::path scriptPath = GetScriptPath("backend/pack_overlay_to_decomp.py");
  std::string cmd = "python \"" + scriptPath.string() + "\" --map " + mapKey;

  int res = std::system(cmd.c_str());
  if (res != 0) {
    Log("[DEPLOY OVERLAY] Failed to pack " + mapKey + " to decomp source.",
        true);
    return false;
  }
  Log("[DEPLOY OVERLAY] Successfully packed " + mapKey +
      " to decomp C source.");
  return true;
}
