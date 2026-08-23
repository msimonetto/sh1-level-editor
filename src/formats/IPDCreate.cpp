#include "formats/IPDCreate.h"
#include "formats/PLMCreate.h"
#include "formats/Structs.h"
#include <vector>
#include <cstdio>
#include <cstring>

bool IPDCreate::CreateBlank(const std::string& outputPath, int8_t xPos, int8_t yPos) {
    std::vector<uint8_t> buf;

    // 1. IPD Header
    buf.resize(sizeof(IPD_FILE_HEADER), 0);
    IPD_FILE_HEADER* hdr = (IPD_FILE_HEADER*)buf.data();
    hdr->id = 0x14;
    hdr->flag = 0;
    hdr->x_pos = xPos;
    hdr->y_pos = yPos;
    hdr->pos_num = 0;
    hdr->obj_num = 0;

    // 2. Collision Header (308 bytes zeroed)
    buf.insert(buf.end(), sizeof(IPD_COLL_HEADER), 0);

    // Re-evaluate pointer after insertion (vector reallocation)
    hdr = (IPD_FILE_HEADER*)buf.data();

    // 3. Object names offset and Object data offset (point immediately after collision)
    hdr->obj_name_offset = buf.size();
    hdr->obj_data_offset = buf.size();
    hdr->unkdata_offset = 0;

    // 4. Generate Blank Embedded PLM
    int plmOffset = buf.size();
    std::vector<uint8_t> plmBuf;
    PLMCreate::GenerateBlankEmbedded(plmBuf);
    buf.insert(buf.end(), plmBuf.begin(), plmBuf.end());

    // Update PLM offset
    hdr = (IPD_FILE_HEADER*)buf.data();
    hdr->plm_offset = plmOffset;

    // Write to disk
    FILE* f = fopen(outputPath.c_str(), "wb");
    if (!f) return false;
    fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
    
    return true;
}
