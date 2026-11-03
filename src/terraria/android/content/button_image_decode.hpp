#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace tefkernel::image {

    /// 解码内存中的 PNG 为 RGBA8（4 通道）。成功返回 true，out 大小 = w*h*4。
    bool decode_png(const void *data, size_t size,
                    std::vector<unsigned char> &out, int &w, int &h);

    /// 读取并解码 PNG 文件。成功返回 true。
    bool decode_png_file(const std::string &path,
                         std::vector<unsigned char> &out, int &w, int &h);

} // namespace tefkernel::image
