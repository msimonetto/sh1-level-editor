#include "formats/PLMCreate.h"
#include <cstdio>
#include <cstring>

bool PLMCreate::CreateBlankGlobal(const std::string& outputPath) {
    std::vector<uint8_t> buf;
    GenerateBlankEmbedded(buf);

    FILE* f = fopen(outputPath.c_str(), "wb");
    if (!f) return false;
    fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
    return true;
}

void PLMCreate::GenerateBlankEmbedded(std::vector<uint8_t>& outBuf) {
    outBuf.clear();
    outBuf.resize(sizeof(PLM_FILE_HEADER), 0);

    PLM_FILE_HEADER* hdr = (PLM_FILE_HEADER*)outBuf.data();
    hdr->id = 0x0630;
    hdr->flag = 0;
    hdr->tex_num = 0;
    hdr->tex_name_offset = sizeof(PLM_FILE_HEADER);
    hdr->obj_num = 0;
    hdr->obj_start_offset = sizeof(PLM_FILE_HEADER);
    hdr->unk_data_offset = 0;
}
