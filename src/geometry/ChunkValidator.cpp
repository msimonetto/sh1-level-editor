#include "geometry/ChunkValidator.h"
#include <cmath>
#include <cstdio>

ValidationResult ChunkValidator::ValidateChunk(const ParsedChunk& chunk) {
    ValidationResult result;

    for (size_t oi = 0; oi < chunk.objects.size(); ++oi) {
        const auto& obj = chunk.objects[oi];
        for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
            const auto& mesh = obj.meshes[mi];
            ValidationResult meshRes = ValidateMesh(mesh, obj, chunk, (int)oi, (int)mi);

            result.errorCount += meshRes.errorCount;
            result.warningCount += meshRes.warningCount;
            result.issues.insert(result.issues.end(), meshRes.issues.begin(), meshRes.issues.end());
        }
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

    // Check 1: Vertex capacity limit
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
        Vector3 pos = { mesh.vx[vi], mesh.vy[vi], mesh.vz[vi] };
        ValidationIssue issue;
        if (!ValidateVertexPosition(pos, chunk.xPos, chunk.yPos, &issue)) {
            issue.chunkName = chunk.chunkName;
            issue.objectIdx = objectIdx;
            issue.meshIdx = meshIdx;
            issue.vertexIdx = (int)vi;

            if (issue.severity == ValidationSeverity::Error) result.errorCount++;
            else result.warningCount++;

            result.issues.push_back(issue);
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
    if (worldPos.y < -VALIDATOR_MAX_HEIGHT) {
        valid = false;
        if (outIssue) {
            outIssue->severity = ValidationSeverity::Warning;
            outIssue->type = ValidationIssueType::HeightBelowMin;
            outIssue->worldPos = worldPos;
            char msg[128];
            snprintf(msg, sizeof(msg), "[HEIGHT] Y=%.2f below min height -%.1ff (16<<8 = 4096 raw units)", 
                     worldPos.y, VALIDATOR_MAX_HEIGHT);
            outIssue->message = msg;
        }
        return false;
    }

    // Chunk XZ Cell bounds + Overhang (7 raw units = 7/256 world units)
    // Note: In Silent Hill 1, Z is inverted (z_world = -(yPos * 40.0f + dta.tz / 256.0f))
    float minX = (float)xPos * VALIDATOR_GRID_SIZE - VALIDATOR_OVERHANG_WORLD;
    float maxX = ((float)xPos + 1.0f) * VALIDATOR_GRID_SIZE + VALIDATOR_OVERHANG_WORLD;
    float minZ = -((float)yPos + 1.0f) * VALIDATOR_GRID_SIZE - VALIDATOR_OVERHANG_WORLD;
    float maxZ = -((float)yPos) * VALIDATOR_GRID_SIZE + VALIDATOR_OVERHANG_WORLD;

    if (worldPos.x < minX || worldPos.x > maxX || worldPos.z < minZ || worldPos.z > maxZ) {
        valid = false;
        if (outIssue) {
            outIssue->severity = ValidationSeverity::Warning;
            outIssue->type = ValidationIssueType::OverhangExceeded;
            outIssue->worldPos = worldPos;
            char msg[128];
            snprintf(msg, sizeof(msg), "[OVERHANG] Pos (%.2f, %.2f) outside chunk cell [%d,%d] (bounds: X:[%.1f,%.1f], Z:[%.1f,%.1f] incl. %d raw overhang)",
                     worldPos.x, worldPos.z, (int)xPos, (int)yPos, minX, maxX, minZ, maxZ, VALIDATOR_OVERHANG_RAW);
            outIssue->message = msg;
        }
        return false;
    }

    return valid;
}

std::pair<int8_t, int8_t> ChunkValidator::DetermineChunkGridPos(Vector3 worldPos) {
    int8_t gx = (int8_t)std::floor(worldPos.x / VALIDATOR_GRID_SIZE);
    int8_t gy = (int8_t)std::floor(-worldPos.z / VALIDATOR_GRID_SIZE);
    return { gx, gy };
}

std::string ChunkValidator::DetermineChunkOwnerForMesh(const RenderMesh& mesh, 
                                                       const RenderObject& obj, 
                                                       const std::vector<ParsedChunk>& allChunks) {
    if (mesh.vx.empty()) return "";

    // Compute mesh centroid in world space
    double sumX = 0, sumY = 0, sumZ = 0;
    for (size_t i = 0; i < mesh.vx.size(); ++i) {
        sumX += mesh.vx[i];
        sumY += mesh.vy[i];
        sumZ += mesh.vz[i];
    }
    Vector3 centroid = {
        (float)(sumX / mesh.vx.size()),
        (float)(sumY / mesh.vx.size()),
        (float)(sumZ / mesh.vx.size())
    };

    auto [gx, gy] = DetermineChunkGridPos(centroid);

    // Look for matching chunk cell in loaded chunks
    for (const auto& chunk : allChunks) {
        if (chunk.xPos == gx && chunk.yPos == gy) {
            return chunk.chunkName;
        }
    }

    return ""; // Not found or in unmapped cell
}
