#pragma once
#include <algorithm>
#include <cmath>

namespace tefkernel::buttons {
struct ScrollWindow {
    int columns=3,rows=9,total=0,row=0;
    bool horizontal=false;
    int capacity() const { return columns*rows; }
    int max_row() const { return std::max(0,(total+columns-1)/columns-rows); }
    void clamp(){ row=std::clamp(row,0,max_row()); }
    void configure(int cols,int lines,bool across=false){columns=std::clamp(cols,1,27);rows=std::clamp(lines,1,27/columns);horizontal=across;clamp();}
    void set_row(int value){row=value;clamp();}
    void set_fraction(float value){set_row(static_cast<int>(std::lround(std::clamp(value,0.0f,1.0f)*max_row())));}
    // Original PE cells are column-major. Overflow adds complete logical rows,
    // leaving every original action in its familiar cell at scroll position 0.
    int source(int cell) const {
        if(cell<0||cell>=capacity())return -1;
        const int col=horizontal?cell%columns:cell/rows;
        const int line=(horizontal?cell/columns:cell%rows)+row;
        const int index=line<rows?(horizontal?line*columns+col:col*rows+line):capacity()+(line-rows)*columns+col;
        return index<total?index:-1;
    }
    int thumb_size(int height) const {
        if(height<=0)return 0;
        if(!max_row())return height;
        return std::min(height,std::max(24,height*rows/(rows+max_row())));
    }
};

struct ScrollGesture {
    int cursor=-1,start_row=0,target_row=0,cell=-1;
    float start_y=0,grab=0;
    bool rail=false,dragging=false;
    bool active() const {return cursor!=-1;}
    bool begin(int id,float y,int row,bool on_rail,int selected,float thumb_grab){
        if(active())return false;
        cursor=id;start_y=y;start_row=target_row=row;rail=on_rail;
        dragging=on_rail;cell=selected;grab=thumb_grab;return true;
    }
    bool move(int id,float y,float pitch,float threshold){
        if(id!=cursor||rail||pitch<=0)return false;
        const float delta=start_y-y;
        if(std::abs(delta)>=threshold)dragging=true;
        if(!dragging)return false;
        target_row=start_row+static_cast<int>(std::lround(delta/pitch));return true;
    }
    bool release(int id){
        if(id!=cursor)return false;
        const bool tap=!dragging&&!rail;cancel();return tap;
    }
    void cancel(){cursor=-1;dragging=false;rail=false;cell=-1;}
};
}
