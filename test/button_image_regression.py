"""Execute production C functions with controlled IL2CPP/image failures on Windows.

Usage: python button_image_regression.py <NDK LLVM bin directory>
The Python runner only compiles/loads the C test; shipped fixes remain pure C.
"""
import ctypes
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
content = root / 'src/terraria/android/content'

def function(path, name):
    source = path.read_text(encoding='utf-8')
    # Definitions follow declarations in button_manager.c.
    import re
    match = list(re.finditer(r'static (?:void|bool) ' + name + r'\([^;]*?\)\s*\{', source))[-1]
    start, cursor, depth = match.start(), match.end(), 1
    while depth:
        if source[cursor] == '{': depth += 1
        if source[cursor] == '}': depth -= 1
        cursor += 1
    return source[start:cursor]

prefix = r'''
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
#define PATCH_NULL NULL
#define BUTTON_LOG_INFO(...) ((void)0)
typedef void *patch_handle_t;
typedef struct { int unused; } patch_method_signature_t;
typedef struct { int unused; } button_action_t;
typedef struct { int x,y,width,height; } button_rect_t;
typedef struct { float x,y; } button_vec2_t;
typedef struct { unsigned char a,b,g,r; } button_color_t;
typedef struct { void *texture; } button_image_entry_t;
static patch_handle_t draw_method;
static void *native_draw;
static bool draw_resolved;
static int checked_ref, bad_ref, image_mode, allocations;
static const int empty_option=-1, carrier_action=27;
static int same(const char *a,const char *b) { while(*a && *a==*b){a++;b++;}return *a==*b; }
static patch_handle_t patchlib_type_get_type(const char *ns,const char *name) {
    (void)ns; if(same(name,"Texture2D"))return (void*)1;
    if(same(name,"Vector2"))return (void*)2;
    if(same(name,"Rectangle"))return (void*)3;
    if(same(name,"Color"))return (void*)4;
    if(same(name,"Single"))return (void*)5;
    if(same(name,"SpriteEffects"))return (void*)6;
    return (void*)7;
}
static patch_handle_t patchlib_type_get_method_by_signature(void *k,const char *n,int count,void *const *types,const char **names) {
    (void)k;(void)n;
    if(count!=9 || !same(names[2],"srcRect") || types[2]!=(void*)3 || types[6]!=(void*)2) return NULL;
    return (void*)10;
}
static void *patchlib_method_get_pointer(void *m){return m;}
static void *il2cpp_method_get_param(void *m,unsigned i){(void)m; return (void*)(uintptr_t)(i+1);}
static bool il2cpp_type_is_byref(const void *p){ checked_ref++;return (bad_ref!=1 && p==(void*)3) || (bad_ref==2 && p==(void*)2); }
typedef struct { char icon_id[8]; } action_t;
/* Test action's representation matches the fields read by the callback. */
#define button_action_t action_t
static action_t action={"png"};
static action_t *action_from_native_id(int id){return id==65536?&action:NULL;}
static void *button_image_texture(const char *id,int *w,int *h){
    allocations++; if(image_mode==0 || (image_mode==1 && !same(id,"@builtin-white")))return NULL;
    *w=96;*h=96;return (void*)20;
}
int _fltused;
static int calls, draw_ok;
static void capture_draw(void *batch,void *texture,button_vec2_t position,
                         button_rect_t *src,uint32_t color,float rotation,
                         button_vec2_t origin,button_vec2_t scale,int effects,
                         float depth,const void *method) {
    (void)color; calls++;
    draw_ok=batch==(void*)30 && texture==(void*)20 && src->x==2 && src->width==8 &&
            position.x==4 && position.y==5 && scale.x==2 && scale.y==3 &&
            rotation==0 && origin.x==0 && origin.y==0 && effects==0 && depth==0 && method==NULL;
}
'''

tests = r'''
int run_tests(void) {
    int failures=0;
    resolve_draw();
    if(!native_draw || !checked_ref) failures |= 1;
    draw_resolved=false; native_draw=NULL; bad_ref=1;
    resolve_draw();
    if(native_draw) failures |= 2;
    draw_resolved=false; native_draw=NULL; bad_ref=2;
    resolve_draw();
    if(native_draw) failures |= 2;
    int id=65536; void *texture=NULL;button_rect_t rect={0};
    void **out_texture=&texture;button_rect_t *out_rect=&rect;
    void *args[]={&id,&out_texture,&out_rect};
    image_mode=0;
    bool skip=native_texture_prefix(NULL,args,NULL,NULL);
    if(skip || id!=carrier_action)failures |= 4;
    id=65536; image_mode=1;
    skip=native_texture_prefix(NULL,args,NULL,NULL);
    if(!skip || texture!=(void*)20 || rect.width!=96)failures |= 8;
    id=65536; image_mode=2; texture=NULL;
    skip=native_texture_prefix(NULL,args,NULL,NULL);
    if(!skip || texture!=(void*)20)failures |= 16;
    native_draw=(void*)capture_draw;
    button_image_entry_t entry={(void*)20};
    button_color_t color={255,255,255,255};
    draw_part((void*)30,&entry,4,5,16,24,&color,(button_rect_t){2,3,8,8});
    if(calls!=1 || !draw_ok) failures |= 32;
    entry.texture=NULL;
    draw_part((void*)30,&entry,4,5,16,24,&color,(button_rect_t){2,3,8,8});
    if(calls!=1) failures |= 64;
    return failures;
}
'''
llvm = Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='button-c-test-') as tmp:
    tmp = Path(tmp)
    cfile, obj, dll = tmp/'regression.c', tmp/'regression.obj', tmp/'regression.dll'
    cfile.write_text(prefix + function(content/'button_images.c','resolve_draw') + '\n' +
                     function(content/'button_manager.c','native_texture_prefix') + '\n' +
                     function(content/'button_images.c','draw_part') + tests, encoding='utf-8')
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-std=c17',
                    '-ffreestanding','-fno-stack-protector','-c',str(cfile),'-o',str(obj)],check=True)
    subprocess.run([str(llvm/'ld.lld.exe'),'-flavor','link','/dll','/noentry',
                    '/export:run_tests','/out:'+str(dll),str(obj)],check=True)
    lib = ctypes.CDLL(str(dll))
    failures = lib.run_tests()
    # Windows keeps a loaded DLL open; release it before TemporaryDirectory cleanup.
    ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(lib._handle))
    if failures:
        raise SystemExit('FAIL: bitmask=%d (1=Draw resolution, 2=byref guard, 4=null fallback, 8=white fallback, 16=PNG, 32=Draw arguments, 64=null draw)' % failures)
    print('PASS: C Draw resolution, byref guards, null fallback, white fallback, PNG texture, Draw arguments, null draw')
