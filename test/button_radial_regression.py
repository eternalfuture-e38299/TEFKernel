"""Run the actual radial C callbacks with a simulated four-slot game binding."""
import ctypes
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/terraria/android/content/button_manager.c').read_text(encoding='utf-8')
def extract(name):
    m = list(re.finditer(r'static bool ' + name + r'\([^;]*?\)\s*\{', source))[-1]
    pos, depth = m.end(), 1
    while depth:
        if source[pos] == '{': depth += 1
        if source[pos] == '}': depth -= 1
        pos += 1
    return source[m.start():pos]

harness = r'''
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#define PATCH_NULL NULL
#define BUTTON_MAX_DRAW_STACK 8
#define BUTTON_MAX_GROUP_BINDINGS 32
typedef void *patch_handle_t;
typedef struct { int unused; } patch_method_signature_t;
void *memcpy(void *d,const void *s,size_t n){unsigned char*a=d;const unsigned char*b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){unsigned char*a=d;while(n--)*a++=(unsigned char)v;return d;}
static void copy_text(char*d,size_t n,const char*s){if(!n)return;while(--n && *s)*d++=*s++;*d=0;}
/* The exact spelling of the input cache key is immaterial to these draw tests. */
static int snprintf(char*d,size_t n,const char*f,...){(void)f;copy_text(d,n,"profile-slot");return 0;}
typedef struct { char key[513],slot[1024];bool multi; } button_draw_context_t;
typedef struct { button_draw_context_t entries[4];size_t next,restore_from; } button_group_draw_t;
typedef struct { button_draw_context_t context;bool radial,pressed; } button_quick_draw_t;
typedef struct { bool used;void *binding,*config,*binding_pin,*config_pin;size_t tick;int slot;char profile[1024]; } button_group_input_t;
typedef struct { void *array;size_t index; } button_render_group_t;
typedef struct { bool visible,held; } button_action_t;
static struct {void *binding_groups,*binding_slot,*slot_multi;} api;
static int state,tick=12;
static const int unbound_action=28,carrier_action=27;
static button_group_draw_t group_drawing[8];static size_t group_drawing_count;
static button_render_group_t render_groups[32];static size_t render_group_count;
static button_quick_draw_t quick_drawing[8];static size_t quick_drawing_count;
static button_draw_context_t drawing[8];static size_t drawing_count;
static bool blocked,in_editor;
static int groups[4],custom[4];
static button_action_t action={true,false};
static bool ui_blocked(void){return blocked;}
static bool editing(void){return in_editor;}
static void *field_read_handle(void*f,void*o){(void)o;return f==api.binding_groups?groups:(void*)100;}
static bool field_read_bool(void*f,void*o){(void)f;(void)o;return false;}
static size_t patchlib_array_length(void*a){(void)a;return 4;}
static bool patchlib_array_at(void*a,size_t i,int*v){*v=((int*)a)[i];return true;}
static bool patchlib_array_set(void*a,size_t i,const int*v){((int*)a)[i]=*v;return true;}
static void copy_managed_string(char*d,size_t n,void*s){(void)s;copy_text(d,n,"slot");}
static void profile_key(char*d,size_t n){copy_text(d,n,"profile");}
static void group_slot(const char*s,int i,char*d,size_t n){(void)s;if(n>1){d[0]='0'+i;d[1]=0;}}
static void custom_binding(void*b,int i,char*d,size_t n){(void)b;if(n>1){d[0]=custom[i]?'A'+i:0;d[1]=0;}}
static button_action_t *button_state_find_action_by_key(void*s,const char*k){(void)s;return *k?&action:NULL;}
static button_group_input_t *group_input_find(const char*k){(void)k;return NULL;}
static button_group_input_t *group_input_allocate(const char*k){(void)k;return NULL;}
static void release_group_input(button_group_input_t*p){(void)p;}
static void *pin(void*p,bool b){(void)b;return p;}
static void *(*il2cpp_gchandle_new)(void*,bool)=pin;
'''

tests = r'''
static void reset(void){
 memset(group_drawing,0,sizeof(group_drawing));group_drawing_count=0;
 quick_drawing_count=0;render_group_count=0;blocked=false;in_editor=false;
 for(int i=0;i<4;i++){groups[i]=unbound_action;custom[i]=0;}
 api.binding_groups=(void*)1;api.binding_slot=(void*)2;api.slot_multi=(void*)3;
}
static void begin_group(void){
 void *binding=(void*)10,*config=(void*)11;int slot=0;
 void *args[]={&binding,&config,&slot};group_prefix(NULL,args,NULL,NULL);
}
static char next_icon(void){
 bool pressed=false,ignore_checks=false,ignore_icon=false;
 void *args[]={NULL,NULL,&pressed,NULL,&ignore_checks,&ignore_icon};
 quick_prefix(NULL,args,NULL,NULL);
 return quick_drawing[quick_drawing_count-1].context.key[0];
}
int run_tests(void){
 int failures=0;
 reset();custom[0]=1;begin_group();
 if(next_icon()!='A')failures|=1;
 if(next_icon()!=0)failures|=2; /* centre must not reuse an outer icon */
 reset();custom[0]=1;custom[3]=1;groups[1]=5;begin_group();
 if(next_icon()!='A' || next_icon()!=0 || next_icon()!='D')failures|=4;
 reset();for(int i=0;i<4;i++)custom[i]=1;begin_group();
 for(int i=0;i<4;i++)if(next_icon()!='A'+i)failures|=8;
 if(next_icon()!=0)failures|=8;
 reset();blocked=true;in_editor=true;custom[2]=1;begin_group();
 if(groups[2]!=carrier_action || next_icon()!='C')failures|=16;
 reset();blocked=true;custom[0]=1;begin_group();
 if(groups[0]!=unbound_action || next_icon()!=0)failures|=32;
 reset();groups[1]=5;begin_group();if(next_icon()!=0)failures|=64;
 reset();blocked=true;in_editor=true;custom[1]=1;
 void *binding=(void*)10;void *args[]={NULL,&binding};bool visible=false;
 if(!group_visible_prefix(NULL,args,NULL,&visible) || !visible)failures|=128;
 blocked=true;in_editor=false;visible=false;
 if(group_visible_prefix(NULL,args,NULL,&visible) || visible)failures|=128;
 return failures;
}
'''
llvm = Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='radial-c-test-') as d:
    d=Path(d);c=d/'test.c';obj=d/'test.obj';dll=d/'test.dll'
    c.write_text(harness+extract('group_visible_prefix')+'\n'+extract('group_prefix')+'\n'+extract('quick_prefix')+tests,encoding='utf-8')
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-std=c17',
                    '-ffreestanding','-fno-stack-protector','-mno-stack-arg-probe','-c',str(c),'-o',str(obj)],check=True)
    subprocess.run([str(llvm/'ld.lld.exe'),'-flavor','link','/dll','/noentry','/export:run_tests','/out:'+str(dll),str(obj)],check=True)
    lib=ctypes.CDLL(str(dll));result=lib.run_tests()
    ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(lib._handle))
    if result:raise SystemExit('FAIL bitmask=%d: 1=single,2=centre,4=mixed,8=four,16=editor,32=blocked,64=vanilla,128=visibility'%result)
    print('PASS: single, centre isolation, mixed vanilla/custom slots, four slots, editor preview, blocked UI, vanilla')
