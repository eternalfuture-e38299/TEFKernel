"""Host integration checks; requires a matching LuaLoader source directory.
Usage: python3 test/run_mod_support_tests.py /path/to/LuaLoader-main
The native game boundary is controlled; this is NOT an Android/device test.
"""
from pathlib import Path
import os,re,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
loader=Path(sys.argv[1]).resolve()
content=root/'src/terraria/android/content'
headers=[root/'include/terraria'/n for n in ['world_entity.h','item_slot.h','recipe_query.h']]
api=[]
for header in headers:
    api += re.findall(r'DEFINE_FUNCTION\(\s*([^,]+),\s*(\w+),\s*([^)]*)\)',header.read_text(),re.S)
with tempfile.TemporaryDirectory(prefix='tef-support-test-') as tmp:
    tmp=Path(tmp)
    generated='#include \"terraria/world_entity.h\"\n#include \"terraria/item_slot.h\"\n#include \"terraria/recipe_query.h\"\n'+'\n'.join(f'{ret} (*bridge_{name})({params});' for ret,name,params in api)
    generated+='\nstatic void bridge_init(void){\n'+''.join(f'bridge_{name}={name};\n' for _,name,_ in api)+'}\n'
    (tmp/'bridge_symbols.h').write_text(generated)
    common=['cc','-std=gnu17','-g','-D__ANDROID__','-I'+str(root/'include'),'-I'+str(root/'src'),'-I'+str(loader/'includes'),'-I'+str(loader/'lib/lua-5.4.8/src'),'-I'+str(tmp)]
    objects=[]
    def compile(source,flags=()):
        obj=tmp/(str(len(objects))+'.o')
        subprocess.run(common+list(flags)+['-c',str(source),'-o',str(obj)],check=True)
        objects.append(obj)
    for name in ['world_entity','world_entity_runtime','item_slot','recipe_query']:
        compile(content/(name+'.c'),['-DIS_TEFKERNEL_BUILD=1','-fsanitize=address,undefined'])
    for name in ['support_runtime_fixture.c','mod_support_test.c']:
        compile(root/'test'/name,['-DIS_TEFKERNEL_BUILD=1','-fsanitize=address,undefined'])
    compile(loader/'src/api/api_support.c',[f'-D{name}=bridge_{name}' for _,name,_ in api]+['-fsanitize=address,undefined'])
    for source in (loader/'lib/lua-5.4.8/src').glob('*.c'):
        compile(source)
    exe=tmp/'test'
    subprocess.run(['cc','-fsanitize=address,undefined',*[str(o) for o in objects],'-pthread','-ldl','-lm','-o',str(exe)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(exe)],check=True,env=env)
