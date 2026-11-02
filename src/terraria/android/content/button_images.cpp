#include "button_images.hpp"
#include "button_viewport.hpp"
#include <chrono>
#include <cstdint>
#include "button_log.hpp"
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "terraria/texture2d.h"
#include "tefstd/vector.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
#include "xdl.h"
#include "button_image_decode.hpp"
namespace tefkernel::gui::images {
void destroy_texture(void *t);
namespace {
struct V {float x,y;};
struct R {int x,y,w,h;};
struct NR {unsigned char yes;unsigned char pad[3];R value;};
struct Entry {std::string path,root,error;int w=0,h=0;void *tex=nullptr;bool attempted=false,reported=false;uint64_t last_use=0;size_t resident=0;
unsigned int pin=0;
};
std::unordered_map<std::string,Entry> cache;
void *draw_method=nullptr;
void *native_draw=nullptr;
bool resolved=false;
void diagnostic(const Entry &e,const std::string &message){
 if(e.root.empty())return;
 std::ofstream out(std::filesystem::path(e.root)/"ui_image_runtime.log",std::ios::app);
 if(out)out<<"[gui-image] "<<e.path<<": "<<message<<'\n';
}
size_t texture_bytes=0;
size_t budget=32*1024*1024,frame_bytes=0;unsigned uploads=0;uint64_t frame=0,evictions=0;
auto frame_start=std::chrono::steady_clock::now();
Viewport clip;
void unload(Entry &e);
bool make_room(size_t bytes,Entry *protect){
 if(bytes>budget)return false;
 while(texture_bytes+bytes>budget){
  Entry *old=nullptr;
  for(auto &kv:cache){auto &e=kv.second;if(&e==protect||!e.resident||e.path=="@builtin-white"||e.last_use+2>=frame)continue;
   if(!old||e.last_use<old->last_use)old=&e;}
  if(!old)return false;unload(*old);++evictions;
 }
 return true;
}
bool can_upload(){return frame==0||(uploads<2 && (uploads==0 || (frame_bytes<4*1024*1024 && std::chrono::steady_clock::now()-frame_start<std::chrono::milliseconds(5))));}
void uploaded(size_t bytes){++uploads;frame_bytes+=bytes;}

using Pin=unsigned int(*)(void*,bool);
using Unpin=void(*)(unsigned int);
Pin pin=nullptr;Unpin unpin=nullptr;
void resolve(){
 if(resolved)return;resolved=true;
 auto texture=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","Texture2D");
 auto sb=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","SpriteBatch");
 auto rect=patchlib_type_get_type("Microsoft.Xna.Framework","Rectangle");
 auto color=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","Color");
 auto vec=patchlib_type_get_type("Microsoft.Xna.Framework","Vector2");
 auto single=patchlib_type_get_type("System","Single");
 auto effects=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","SpriteEffects");
 void *types[]={texture,vec,rect,color,single,vec,vec,effects,single};
 const char *names[]={"texture","position","srcRect","color","rotation","origin","scale","effects","layerDepth"};
 draw_method=patchlib_type_get_method_by_signature(sb,"Draw",9,types,names);
 if(draw_method)native_draw=patchlib_method_get_pointer(draw_method);
 void *lib=dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);
 pin=reinterpret_cast<Pin>(dlsym(lib?lib:RTLD_DEFAULT,"il2cpp_gchandle_new"));
 unpin=reinterpret_cast<Unpin>(dlsym(lib?lib:RTLD_DEFAULT,"il2cpp_gchandle_free"));
 if(lib)dlclose(lib);
 if(!pin || !unpin){
    void *extended=xdl_open("libil2cpp.so",XDL_DEFAULT);
    if(extended){
        pin=reinterpret_cast<Pin>(xdl_sym(extended,"il2cpp_gchandle_new",nullptr));
        unpin=reinterpret_cast<Unpin>(xdl_sym(extended,"il2cpp_gchandle_free",nullptr));
        xdl_close(extended);
    }
 }
 LOG_INFO("[gui-image] draw interface={} texture API={}",draw_method!=nullptr,terraria_texture2d_create!=nullptr);
}
bool load(Entry &e){
 if(e.attempted)return e.tex!=nullptr;
 if(!can_upload())return false;
 e.attempted=true;resolve();
 if(!draw_method || !terraria_texture2d_create || !terraria_texture2d_get_class || !terraria_texture2d_get_class()){e.error="image API unavailable";return false;}
 if(!pin || !unpin){e.error="IL2CPP GC retention unavailable";return false;}
 const auto ut=patchlib_type_get_type("UnityEngine","Texture2D");
 const auto xt=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","Texture2D");
 const auto texture_base=patchlib_type_get_type("UnityEngine","Texture");
 if(!ut || !xt || !texture_base || !patchlib_type_get_method_by_param_count(ut,".ctor",4) ||
    !patchlib_type_get_method_by_param_count(texture_base,"set_filterMode",1) ||
    !patchlib_type_get_method_by_param_count(texture_base,"set_wrapMode",1) ||
    !patchlib_type_get_method_by_param_count(ut,"GetWritableImageData",1) ||
    !patchlib_type_get_method_by_param_count(ut,"GetRawImageDataSize",0) ||
    !patchlib_type_get_method_by_param_count(ut,"Apply",0) ||
    !patchlib_type_get_method_by_param_types(xt,".ctor",1,&ut)){
    e.error="Unity texture upload interface unavailable";return false;
 }
  std::vector<unsigned char> pixbuf;
 if(e.path=="@builtin-white"){
  e.w=e.h=1;pixbuf.assign(4,255);
 }else {
 std::ifstream file(e.path,std::ios::binary|std::ios::ate);
 if(!file){e.error="PNG file missing";return false;}
 const auto size=file.tellg();if(size<=0 || size>16*1024*1024){e.error="PNG size limit";return false;}
 std::vector<unsigned char> bytes(static_cast<size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
 if(!image::decode_png(bytes.data(),bytes.size(),pixbuf,e.w,e.h) || e.w<1 || e.h<1 || e.w>2048 || e.h>2048){e.error="invalid PNG or over 2048 pixels";return false;}
 size_t need=static_cast<size_t>(e.w)*e.h*4;
 if(!make_room(need,&e)){e.attempted=false;return false;}
 }
 unsigned char *pixels=pixbuf.data();
 const size_t count=static_cast<size_t>(e.w)*e.h*4;
 // XNA AlphaBlend expects premultiplied alpha.
 for(size_t i=0;i<count;i+=4){for(int j=0;j<3;++j)pixels[i+j]=static_cast<unsigned char>((pixels[i+j]*pixels[i+3]+127)/255);}
 // Unity raw pixel data is bottom-up; Lua source rectangles use top-left coordinates.
 for(int y=0;y<e.h/2;++y)for(int x=0;x<e.w*4;++x)std::swap(pixels[y*e.w*4+x],pixels[(e.h-y-1)*e.w*4+x]);
 try { e.tex=terraria_texture2d_create(e.w,e.h,TEXTURE_FORMAT_RGBA32,pixels,count); }
 catch(...){e.error="texture upload exception";}
 if(!e.tex){if(e.error.empty())e.error="texture upload failed";return false;}
 e.pin=pin(e.tex,false);if(!e.pin){destroy_texture(e.tex);e.tex=nullptr;e.error="texture GC retention failed";return false;}
 diagnostic(e,"loaded "+std::to_string(e.w)+"x"+std::to_string(e.h));
 e.resident=count;
 texture_bytes+=e.resident;uploaded(count);
 LOG_INFO("[gui-image] loaded {} ({}x{})",e.path,e.w,e.h);return true;
}
void part(void *sb,Entry &e,float x,float y,float w,float h,const void *color,R source){
 if(w<=0||h<=0||source.w<=0||source.h<=0)return;
 if(clip.enabled){
  float ox=x,oy=y,ow=w,oh=h;if(!clip.crop(x,y,w,h))return;
  int left=static_cast<int>(std::floor((x-ox)*source.w/ow));
  int top=static_cast<int>(std::floor((y-oy)*source.h/oh));
  int right=std::min(source.w,static_cast<int>(std::ceil((x+w-ox)*source.w/ow)));
  int bottom=std::min(source.h,static_cast<int>(std::ceil((y+h-oy)*source.h/oh)));
  source.x+=left;source.y+=top;source.w=right-left;source.h=bottom-top;
  if(source.w<=0||source.h<=0)return;
 }

 using Fn=void(*)(void*,void*,V,R*,unsigned int,float,V,V,int,float);
 auto fn=reinterpret_cast<Fn>(native_draw);
 unsigned char tint[4];std::memcpy(tint,color,4);
 for(int i=1;i<4;++i)tint[i]=static_cast<unsigned char>((tint[i]*tint[0]+127)/255);
 unsigned int packed;std::memcpy(&packed,tint,4);
 if(fn)fn(sb,e.tex,V{x,y},&source,packed,0.0f,V{0,0},V{w/source.w,h/source.h},0,0.0f);
}
}
std::string request(const std::string &owner,const std::string &root,const std::string &path){
 const std::string id=owner+"#"+path;
 if(cache.count(id))return id;
 namespace fs=std::filesystem;
 fs::path rel(path);if(rel.empty()||rel.is_absolute()||rel.extension()!=".png")return {};
 for(const auto &p:rel)if(p=="..")return {};
 std::error_code ec;auto base=fs::weakly_canonical(root,ec);if(ec)return {};
 auto full=fs::weakly_canonical(base/rel,ec);if(ec)return {};
 auto check=full.lexically_relative(base);for(const auto &p:check)if(p=="..")return {};

 if(!cache.count(id)){
  if(cache.size()>=4096)return {};Entry e;e.path=full.string();e.root=base.string();cache.emplace(id,std::move(e));
 }
 return id;
}
std::string status(const std::string &id,int &w,int &h){auto it=cache.find(id);if(it==cache.end())return "invalid";w=it->second.w;h=it->second.h;return !it->second.attempted?"pending":it->second.tex?"ready":"failed: "+it->second.error;}
void draw(void *sb,const std::string &id,float x,float y,float w,float h,const void *color,int sx,int sy,int sw,int sh,int border,float edge){
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||!std::isfinite(edge)||w<=0||h<=0)return;
 if(!clip.intersects(x,y,w,h))return;
 auto it=cache.find(id);if(it==cache.end()||!sb)return;auto &e=it->second;e.last_use=frame;
 try{
  if(!load(e)){if(e.attempted&&!e.reported){LOG_WARN("[gui-image] {}: {}",e.path,e.error);diagnostic(e,e.error);e.reported=true;}return;}
  if(sw<=0)sw=e.w;if(sh<=0)sh=e.h;
  if(sx<0||sy<0||sw<=0||sh<=0||sx>e.w||sy>e.h||sw>e.w-sx||sh>e.h-sy)return;
  if(border==-1){const float factor=std::min(w/sw,h/sh);const float nw=sw*factor,nh=sh*factor;
    part(sb,e,x+(w-nw)/2,y+(h-nh)/2,nw,nh,color,R{sx,sy,sw,sh});return;}
  if(border==-2){const float factor=std::max(w/sw,h/sh);const int nw=std::max(1,static_cast<int>(w/factor)),nh=std::max(1,static_cast<int>(h/factor));
    part(sb,e,x,y,w,h,color,R{sx+(sw-nw)/2,sy+(sh-nh)/2,nw,nh});return;}
  if(border<=0){part(sb,e,x,y,w,h,color,R{sx,sy,sw,sh});return;}
  const int b=std::min(border,std::min(sw,sh)/2);if(b<=0)return;
  const float target=edge>0?edge:static_cast<float>(b);
  const float dx=std::min(target,w/2),dy=std::min(target,h/2);
  const int xs[]={sx,sx+b,sx+sw-b},ys[]={sy,sy+b,sy+sh-b};
  const int ws[]={b,sw-2*b,b},hs[]={b,sh-2*b,b};
  const float xx[]={x,x+dx,x+w-dx},yy[]={y,y+dy,y+h-dy};
  const float ww[]={dx,w-2*dx,dx},hh[]={dy,h-2*dy,dy};
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)part(sb,e,xx[col],yy[row],ww[col],hh[row],color,R{xs[col],ys[row],ws[col],hs[row]});
 }catch(...){LOG_ERROR("[gui-image] managed drawing exception for {}",id);if(!e.reported){diagnostic(e,"managed drawing exception");e.reported=true;}}
}
void solid(void *sb,float x,float y,float w,float h,const void *color){
 if(!cache.count("@builtin-white")){Entry e;e.path="@builtin-white";cache.emplace("@builtin-white",std::move(e));}
 draw(sb,"@builtin-white",x,y,w,h,color);
}
void *dispose_method(){
 auto type=patchlib_type_get_type("Microsoft.Xna.Framework.Graphics","Texture2D");
 for(int i=0;type && i<8;++i){auto method=patchlib_type_get_method_by_param_count(type,"Dispose",0);if(method)return method;type=patchlib_type_get_parent(type);}
 return nullptr;
}
void destroy_texture(void *t){
 if(!t)return;auto dispose=dispose_method();
 if(dispose){try{patchlib_method_invoke_args(dispose,t,nullptr,nullptr);}catch(...){}}
}
namespace {
void unload(Entry &e){
 destroy_texture(e.tex);
 if(unpin&&e.pin)unpin(e.pin);e.pin=0;
 texture_bytes=texture_bytes>=e.resident?texture_bytes-e.resident:0;
 e.resident=0;e.tex=nullptr;e.attempted=false;e.reported=false;e.error.clear();
}
}
void release(const std::string &id){auto it=cache.find(id);if(it==cache.end())return;unload(it->second);cache.erase(it);}
unsigned int retain_object(void *object){
 resolve();return pin?pin(object,false):0;
}
void release_object(unsigned int handle){
 if(unpin&&handle)unpin(handle);
}
void begin_frame(){++frame;uploads=0;frame_bytes=0;frame_start=std::chrono::steady_clock::now();clip={};}
void set_clip(bool enabled,float x,float y,float w,float h){clip={enabled,x,y,w,h};}
void set_budget(size_t bytes){budget=std::clamp(bytes,size_t(8*1024*1024),size_t(64*1024*1024));}
void stats(size_t &used,size_t &limit,size_t &entries,uint64_t &removed){used=texture_bytes;limit=budget;entries=cache.size();removed=evictions;}
void prune(const std::vector<std::string> &owners){
 std::vector<std::string> remove;
 for(const auto &kv:cache){if(kv.first=="@builtin-white")continue;bool live=false;for(const auto &owner:owners)if(kv.first.rfind(owner+"#",0)==0){live=true;break;}if(!live)remove.push_back(kv.first);}
 for(const auto &id:remove)release(id);
}
void clear(){std::vector<std::string> ids;for(auto &kv:cache)ids.push_back(kv.first);for(auto &id:ids)release(id);
 cache.clear();texture_bytes=0;frame=evictions=0;clip={};draw_method=nullptr;resolved=false;
 native_draw=nullptr;
}
}

namespace tefkernel::gui::images {
void* texture(const std::string& id,int& w,int& h){
 if(id=="@builtin-white"&&!cache.count(id)){Entry e;e.path=id;cache.emplace(id,std::move(e));}
 auto it=cache.find(id);if(it==cache.end())return nullptr;auto& e=it->second;e.last_use=frame;
 if(!load(e))return nullptr;w=e.w;h=e.h;return e.tex;
}
}
