#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace tefkernel::gui::images {
std::string request(const std::string &owner,const std::string &root,const std::string &path);
std::string status(const std::string &id,int &w,int &h);
void draw(void *sprite_batch,const std::string &id,float x,float y,float w,float h,
          const void *color,int sx=0,int sy=0,int sw=0,int sh=0,int border=0,float edge=0);
void release(const std::string &id);
void prune(const std::vector<std::string> &owners);
void solid(void *sprite_batch,float x,float y,float w,float h,const void *color);
void* retain_object(void *object);
void release_object(void *handle);
void begin_frame();
void set_clip(bool enabled,float x,float y,float w,float h);
void set_budget(size_t bytes);
void stats(size_t &used,size_t &limit,size_t &entries,uint64_t &evictions);
void clear();
}

namespace tefkernel::gui::images { void* texture(const std::string& id,int& w,int& h); }
