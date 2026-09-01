#include "geometry/ChunkValidator.h"
#include "geometry/GeometryCommon.h"
#include "viewport/LocalGeometryOverlay.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace Geometry;

ValidationResult ChunkValidator::ValidateChunk(const ParsedChunk& chunk, 
                                              const std::vector<ParsedChunk>* allLoadedChunks) {
    ValidationResult result;

    for (size_t oi = 0; oi < chunk.objects.size(); ++oi) {
        const auto& obj = chunk.objects[oi];
        ValidationResult objRes = ValidateObject(obj, chunk, static_cast<int>(oi), allLoadedChunks);

        result.errorCount += objRes.errorCount;
        result.warningCount += objRes.warningCount;
        result.issues.insert(result.issues.end(), objRes.issues.begin(), objRes.issues.end());
    }

    return result;
}

ValidationResult ChunkValidator::ValidateObject(const RenderObject& obj, 
                                               const ParsedChunk& chunk, 
                                               int objectIdx, 
                                               const std::vector<ParsedChunk>* allLoadedChunks) {
    ValidationResult result;

    // 1. Check Global Object Anchor alignment
    if (obj.isGlobal) {
        float worldX = (static_cast<float>(obj.rawTx) + 10240.0f * static_cast<float>(chunk.xPos)) * (1.0f / 256.0f);
        float worldZ = -(static_cast<float>(obj.rawTz) + 10240.0f * static_cast<float>(chunk.yPos)) * (1.0f / 256.0f);
        ChunkGridCell anchorCell = ChunkOwnership::WorldToGridPos({ worldX, 0.0f, worldZ });

        if (anchorCell.x != chunk.xPos || anchorCell.y != chunk.yPos) {
            std::string suggestedChunk = ChunkOwnership::FormatChunkName(chunk.chunkPrefix, anchorCell.x, anchorCell.y);
            ValidationIssue issue;
            issue.severity = ValidationSeverity::Warning;
            issue.type = ValidationIssueType::GlobalObjectMisplaced;
            issue.chunkName = chunk.chunkName;
            issue.objectIdx = objectIdx;
            issue.worldPos = { worldX, -static_cast<float>(obj.rawTy) * (1.0f / 256.0f), worldZ };
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "[PROP OWNER] Prop '%s' anchor (%.2f, %.2f) is in cell [%d,%d] (%s) but belongs to chunk '%s' [%d,%d]",
                     obj.name.c_str(), worldX, worldZ,
                     static_cast<int>(anchorCell.x), static_cast<int>(anchorCell.y),
                     suggestedChunk.c_str(), chunk.chunkName.c_str(),
                     static_cast<int>(chunk.xPos), static_cast<int>(chunk.yPos));
            issue.message = msg;
            result.issues.push_back(issue);
            result.warningCount++;
        }
    }

    // 2. Check individual meshes
    for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
        const auto& mesh = obj.meshes[mi];
        ValidationResult meshRes = ValidateMesh(mesh, obj, chunk, objectIdx, static_cast<int>(mi));

        result.errorCount += meshRes.errorCount;
        result.warningCount += meshRes.warningCount;
        result.issues.insert(result.issues.end(), meshRes.issues.begin(), meshRes.issues.end());
    }

    return result;
}

ValidationResult ChunkValidator::ValidateMesh(const RenderMesh& mesh, 
                                              const RenderObject& obj, 
                                              const ParsedChunk& chunk, 
                                              int objectIdx, 
                                              int meshIdx) {
    ValidationResult result;
    size_t vertCount = mesh.vx.size();

    // Check 1: Vertex capacity limit (PS1 uint8 limit)
    if (vertCount > VALIDATOR_MAX_VERTS) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::Error;
        issue.type = ValidationIssueType::VertexCapacityExceeded;
        issue.chunkName = chunk.chunkName;
        issue.objectIdx = objectIdx;
        issue.meshIdx = meshIdx;
        char msg[128];
        snprintf(msg, sizeof(msg), "[CRITICAL] Mesh '%s' [%d] has %zu vertices, exceeding PS1 limit of %d",
                 obj.name.c_str(), meshIdx, vertCount, VALIDATOR_MAX_VERTS);
        issue.message = msg;
        result.issues.push_back(issue);
        result.errorCount++;
    } else if (vertCount >= VALIDATOR_WARN_VERTS) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::Warning;
        issue.type = ValidationIssueType::VertexCapacityNearLimit;
        issue.chunkName = chunk.chunkName;
        issue.objectIdx = objectIdx;
        issue.meshIdx = meshIdx;
        char msg[128];
        snprintf(msg, sizeof(msg), "[WARNING] Mesh '%s' [%d] vertex count (%zu / %d) near capacity",
                 obj.name.c_str(), meshIdx, vertCount, VALIDATOR_MAX_VERTS);
        issue.message = msg;
        result.issues.push_back(issue);
        result.warningCount++;
    }

    // Check 2: Vertex positions (Height & Cell Overhang)
    for (size_t vi = 0; vi < vertCount; ++vi) {
        Vector3 pos = GetMeshVertex(mesh, vi);
        ValidationIssue issue;
        if (!ValidateVertexPosition(pos, chunk.xPos, chunk.yPos, &issue)) {
            issue.chunkName = chunk.chunkName;
            issue.objectIdx = objectIdx;
            issue.meshIdx = meshIdx;
            issue.vertexIdx = static_cast<int>(vi);

            if (issue.severity == ValidationSeverity::Error) result.errorCount++;
            else result.warningCount++;

            result.issues.push_back(issue);
        }
    }

    // Check 3: Chunk Ownership & Cross-Chunk Placement
    ChunkOccupancy occ = ChunkOwnership::AnalyzeMeshOccupancy(mesh, chunk.xPos, chunk.yPos, chunk.chunkPrefix);
    if (occ.spansMultipleChunks) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::Warning;
        issue.type = ValidationIssueType::CrossChunkMesh;
        issue.chunkName = chunk.chunkName;
        issue.objectIdx = objectIdx;
        issue.meshIdx = meshIdx;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[CROSS-CHUNK] Mesh '%s' [%d] spans %zu chunk cells (dominant: %s). PS1 will suffer culling glitches.",
                 obj.name.c_str(), meshIdx, occ.occupiedCells.size(), occ.suggestedChunkName.c_str());
        issue.message = msg;
        result.issues.push_back(issue);
        result.warningCount++;
    } else if (!occ.IsOwnedBy(chunk.xPos, chunk.yPos) && !mesh.vx.empty()) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::Warning;
        issue.type = ValidationIssueType::WrongChunkOwner;
        issue.chunkName = chunk.chunkName;
        issue.objectIdx = objectIdx;
        issue.meshIdx = meshIdx;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[MISPLACED] Mesh '%s' [%d] is located in cell [%d,%d] (%s) but belongs to chunk '%s' [%d,%d]",
                 obj.name.c_str(), meshIdx,
                 static_cast<int>(occ.dominantCell.x), static_cast<int>(occ.dominantCell.y),
                 occ.suggestedChunkName.c_str(), chunk.chunkName.c_str(),
                 static_cast<int>(chunk.xPos), static_cast<int>(chunk.yPos));
        issue.message = msg;
        result.issues.push_back(issue);
        result.warningCount++;
    }

    // Check 4: Face Topology & Texture Range Validity
    const auto& texList = obj.isGlobal ? chunk.globalTexNames : chunk.localTexNames;
    for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
        const auto& face = mesh.faces[fi];
        bool isQuad = (face.v[3] != 0xFF);
        int vCount = isQuad ? 4 : 3;

        // Check index validity
        bool hasBadIndex = false;
        for (int k = 0; k < vCount; ++k) {
            if (face.v[k] >= vertCount) {
                ValidationIssue issue;
                issue.severity = ValidationSeverity::Error;
                issue.type = ValidationIssueType::InvalidVertexIndex;
                issue.chunkName = chunk.chunkName;
                issue.objectIdx = objectIdx;
                issue.meshIdx = meshIdx;
                issue.faceIdx = static_cast<int>(fi);
                char msg[128];
                snprintf(msg, sizeof(msg), "[TOPOLOGY] Face #%zu references vertex index %d out of bounds (%zu verts)",
                         fi, face.v[k], vertCount);
                issue.message = msg;
                result.issues.push_back(issue);
                result.errorCount++;
                hasBadIndex = true;
            }
        }

        // Check degenerate faces
        if (!hasBadIndex) {
            bool degenerate = false;
            if (!isQuad) {
                if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[2] == face.v[0]) {
                    degenerate = true;
                }
            } else {
                std::set<uint8_t> uniqueVerts = { face.v[0], face.v[1], face.v[2], face.v[3] };
                if (uniqueVerts.size() < 3) {
                    degenerate = true;
                }
            }

            if (degenerate) {
                ValidationIssue issue;
                issue.severity = ValidationSeverity::Warning;
                issue.type = ValidationIssueType::DegenerateFace;
                issue.chunkName = chunk.chunkName;
                issue.objectIdx = objectIdx;
                issue.meshIdx = meshIdx;
                issue.faceIdx = static_cast<int>(fi);
                char msg[128];
                snprintf(msg, sizeof(msg), "[TOPOLOGY] Face #%zu is degenerate (duplicate/collapsed vertex indices)", fi);
                issue.message = msg;
                result.issues.push_back(issue);
                result.warningCount++;
            }
        }

        // Check texture index
        if (face.texNum != 0x7F && face.texNum >= static_cast<int>(texList.size())) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::Warning;
            issue.type = ValidationIssueType::TextureIndexOutOfRange;
            issue.chunkName = chunk.chunkName;
            issue.objectIdx = objectIdx;
            issue.meshIdx = meshIdx;
            issue.faceIdx = static_cast<int>(fi);
            char msg[128];
            snprintf(msg, sizeof(msg), "[TEXTURE] Face #%zu references texture 0x%02X exceeding texture count (%zu)",
                     fi, face.texNum, texList.size());
            issue.message = msg;
            result.issues.push_back(issue);
            result.warningCount++;
        }
    }

    return result;
}

bool ChunkValidator::ValidateVertexPosition(Vector3 worldPos, 
                                            int8_t xPos, 
                                            int8_t yPos, 
                                            ValidationIssue* outIssue) {
    bool valid = true;

    // Height bounds (-16.0f to +16.0f world units; 16<<8 = 4096 raw units)
    if (worldPos.y > VALIDATOR_MAX_HEIGHT) {
        valid = false;
        if (outIssue) {
            outIssue->severity = ValidationSeverity::Warning;
            outIssue->type = ValidationIssueType::HeightExceeded;
            outIssue->worldPos = worldPos;
            char msg[128];
            snprintf(msg, sizeof(msg), "[HEIGHT] Y=%.2f exceeds max height limit %.1ff (16<<8 = 4096 raw units)",
                     worldPos.y, VALIDATOR_MAX_HEIGHT);
            outIssue->message = msg;
        }
        return false;
    }
    if (worldPos.y < VALIDATOR_MIN_HEIGHT) {
        valid = false;
        if (outIssue) {
            outIssue->severity = ValidationSeverity::Warning;
            outIssue->type = ValidationIssueType::HeightBelowMin;
            outIssue->worldPos = worldPos;
            char msg[128];
            snprintf(msg, sizeof(msg), "[HEIGHT] Y=%.2f below min height %.1ff (16<<8 = 4096 raw units)", 
                     worldPos.y, VALIDATOR_MIN_HEIGHT);
            outIssue->message = msg;
        }
        return false;
    }

    // Chunk XZ Cell bounds + Overhang (16 raw units = 16/256 world units)
    BoundingBox bounds = ChunkOwnership::GridToWorldBounds(xPos, yPos, true);

    if (worldPos.x < bounds.min.x || worldPos.x > bounds.max.x || 
        worldPos.z < bounds.min.z || worldPos.z > bounds.max.z) {
        valid = false;
        if (outIssue) {
            outIssue->severity = ValidationSeverity::Warning;
            outIssue->type = ValidationIssueType::OverhangExceeded;
            outIssue->worldPos = worldPos;
            char msg[140];
            snprintf(msg, sizeof(msg), "[OVERHANG] Pos (%.2f, %.2f) outside chunk cell [%d,%d] (bounds: X:[%.1f,%.1f], Z:[%.1f,%.1f])",
                     worldPos.x, worldPos.z, static_cast<int>(xPos), static_cast<int>(yPos),
                     bounds.min.x, bounds.max.x, bounds.min.z, bounds.max.z);
            outIssue->message = msg;
        }
        return false;
    }

    return valid;
}

ValidationResult ChunkValidator::ValidateLoadedChunks(const std::vector<LoadedChunk>& chunks) {
    ValidationResult totalResult;
    std::vector<ParsedChunk> parsedList;
    for (const auto& lc : chunks) {
        if (lc.data) parsedList.push_back(*lc.data);
    }

    for (const auto& lc : chunks) {
        if (!lc.data) continue;
        ValidationResult chunkRes = ValidateChunk(*lc.data, &parsedList);
        totalResult.errorCount += chunkRes.errorCount;
        totalResult.warningCount += chunkRes.warningCount;
        totalResult.issues.insert(totalResult.issues.end(), chunkRes.issues.begin(), chunkRes.issues.end());
    }

    return totalResult;
}

bool ValidationResult::HasErrors() const {
    return errorCount > 0;
}

bool ValidationResult::HasWarnings() const {
    return warningCount > 0;
}

bool ValidationResult::IsClean() const {
    return errorCount == 0 && warningCount == 0;
}

Geometry::ChunkGridCell ChunkValidator::DetermineChunkGridPos(Vector3 worldPos) {
    return Geometry::ChunkOwnership::WorldToGridPos(worldPos);
}

std::string ChunkValidator::DetermineChunkOwnerForMesh(const RenderMesh& mesh, 
                                                       const RenderObject& obj, 
                                                       const std::vector<ParsedChunk>& allChunks) {
    return Geometry::ChunkOwnership::DetermineMeshOwner(mesh, 0, 0, "", &allChunks);
}
