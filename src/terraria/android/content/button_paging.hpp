#pragma once
#include <algorithm>
#include <cstddef>
namespace tefkernel::buttons {
constexpr size_t native_grid_capacity=27;
constexpr size_t native_page_capacity=native_grid_capacity-2;
inline int page_count(size_t total){return total<=native_grid_capacity?1:static_cast<int>((total+native_page_capacity-1)/native_page_capacity);}
inline size_t page_begin(size_t total,int page){return page_count(total)==1?0:static_cast<size_t>(std::clamp(page,0,page_count(total)-1))*native_page_capacity;}
inline size_t page_end(size_t total,int page){return std::min(total,page_begin(total,page)+(page_count(total)==1?native_grid_capacity:native_page_capacity));}
}
