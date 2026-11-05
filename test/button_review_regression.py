"""Portable host checks for PE button identity and decoded image registration."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
content = root / 'src/terraria/android/content'
manager = (content / 'button_manager.c').read_text()
start = manager.index('static bool button_scope_key(')
end = manager.index('\nbool terraria_button_manager_register', start)
scope_function = manager[start:end]
source = r'''
#include <assert.h>
#include <string.h>
#include "terraria/content/button_manager.h"
#include "terraria/android/content/button_state.h"
#include "terraria/android/content/button_images.c"
''' + scope_function + r'''
extern int dispose_lookups, dispose_calls;
int main(void) {
    char a[130], b[130], c[130];
    terraria_button_scope_t first={"loader.a", "mod.a"};
    terraria_button_scope_t second={"loader.b", "mod.a"};
    terraria_button_scope_t third={"loader.a", "mod.b"};
    assert(button_scope_key(&first,a));
    assert(button_scope_key(&second,b));
    assert(button_scope_key(&third,c));
    assert(strcmp(a,b) && strcmp(a,c));
    assert(!button_scope_key(NULL,a));
    terraria_button_scope_t bad={"loader:a", "mod.a"};
    assert(!button_scope_key(&bad,c));
    button_state_t state;
    button_state_init(&state);
    assert(button_state_register_action(&state,a,"attack","A","","",true));
    assert(button_state_register_action(&state,b,"attack","B","","",true));
    button_state_unregister_owner(&state,a);
    assert(!button_state_find_action(&state,a,"attack"));
    assert(button_state_find_action(&state,b,"attack"));
    button_state_destroy(&state);
    unsigned char rgba[]={200,100,50,128, 20,30,40,255};
    assert(!button_image_request("bad",rgba,7,1,2));
    assert(!button_image_request("bad",NULL,8,1,2));
    assert(!button_image_request("bad",rgba,8,-1,2));
    assert(!button_image_request("bad",rgba,8,2049,2));
    assert(button_image_request("icon",rgba,8,1,2));
    assert(!button_image_request("icon",rgba,8,1,2));
    button_image_entry_t *entry=find_image("icon");
    assert(entry && entry->pixels != rgba && pixel_bytes==8);
    unsigned char expected[]={20,30,40,255,100,50,25,128};
    assert(memcmp(entry->pixels,expected,8)==0);
    memset(rgba,0,8);
    assert(memcmp(entry->pixels,expected,8)==0);
    /* No texture was uploaded; release must still free the retained pixels. */
    button_image_release("icon");
    assert(image_count==0 && pixel_bytes==0);
    /* Disposal reflection is shared across textures, reset on clear. */
    destroy_texture((void*)1);
    destroy_texture((void*)2);
    assert(dispose_lookups==1 && dispose_calls==2);
    button_image_clear();
    destroy_texture((void*)3);
    assert(dispose_lookups==2 && dispose_calls==3);
    return 0;
}
'''
# Supply link-only runtime symbols; graphics upload is deliberately not invoked.
# gc-sections discards unrelated rendering and IL2CPP methods.
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    (tmp/'test.c').write_text(source)
    # Release references the game's disposal API even for a not-yet-uploaded image.
    # Use real image code with these runtime hooks unreachable in this host test.
    (tmp/'stubs.c').write_text(r'''
#include "patchlib/il2cpp_api.h"
#include "patchlib/type.h"
#include "patchlib/method.h"
int dispose_lookups, dispose_calls;
patch_handle_t patchlib_type_get_type(const char *a,const char *b){return (void*)1;}
patch_handle_t patchlib_type_get_parent(patch_handle_t a){return 0;}
patch_handle_t patchlib_type_get_method_by_param_count(patch_handle_t a,const char *b,int c){++dispose_lookups;return (void*)2;}
bool patchlib_method_invoke_args(patch_handle_t a,patch_handle_t b,void *c,void **d){++dispose_calls;return true;}
void (*il2cpp_gchandle_free)(void *);
''')
    subprocess.run(['cc','-std=c17','-DIS_TEFKERNEL_BUILD=1','-D__ANDROID__',
        '-ffunction-sections','-fdata-sections','-I'+str(root/'include'),'-I'+str(root/'src'),
        str(tmp/'test.c'),str(tmp/'stubs.c'),str(content/'button_state.c'),
        '-Wl,--gc-sections','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('PASS: scoped isolation, invalid identities, RGBA validation/copy/conversion/release, disposal cache/reset')
