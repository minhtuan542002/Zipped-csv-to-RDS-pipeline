#pragma once

#include <string>
#include <iostream> 
#include <fstream> 
#include <vector>
#include <filesystem>
#include <zip.h>

static bool ends_with(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool extractFile(zip_t* archive, zip_int32_t index, const std::shared_ptr<Aws::StringStream> fileData) {
    struct zip_file* zf = zip_fopen_index(archive, index, 0);
    
    struct zip_stat st;
    zip_stat_init(&st);

    zip_int64_t bufferSize;
    if (zip_stat_index(archive, index, 0, &st) == 0) { 
        bufferSize = st.size;
        std::cout << "Successfully retrieved file info of index: " << index << std::endl;
    }
    else { 
        std::cerr << "Failed to get file stats: " << zip_strerror(archive) << std::endl;
        return false;
    }

    std::vector<char> buffer(bufferSize);
    while ((zip_fread(zf, buffer.data(), bufferSize)) > 0) {
        fileData->write(buffer.data(), bufferSize);
        std::cout << "Successfully written content of index: " << index << std::endl;
    }

    zip_fclose(zf);
    return true;
}
