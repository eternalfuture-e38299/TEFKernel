#pragma once
#include <algorithm>
#include <cmath>
namespace tefkernel::gui {
struct Viewport {
 bool enabled=false;float x=0,y=0,w=0,h=0;
 bool contains(float px,float py)const{return !enabled||(px>=x&&py>=y&&px<x+w&&py<y+h);}
 bool intersects(float px,float py,float pw,float ph)const{return !enabled||(px<x+w&&py<y+h&&px+pw>x&&py+ph>y);}
 bool covers(float px,float py,float pw,float ph)const{return !enabled||(px>=x&&py>=y&&px+pw<=x+w&&py+ph<=y+h);}
 bool crop(float &px,float &py,float &pw,float &ph)const {
  if(!enabled)return pw>0&&ph>0;
  float right=std::min(px+pw,x+w),bottom=std::min(py+ph,y+h);
  px=std::max(px,x);py=std::max(py,y);pw=right-px;ph=bottom-py;return pw>0&&ph>0;
 }
};
}
