/*******************************************************************************
 * tefkernel - Basic.cs
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/05/17
 *******************************************************************************/

using System.Runtime.InteropServices;

namespace tefloader.Il2CppApi;

public static class Basic
{
    public static IntPtr il2cpp_domain_get()
    {
        var domain = AppDomain.CurrentDomain;
        var handle = GCHandle.Alloc(domain);
        return GCHandle.ToIntPtr(handle);
    }
    
    public static unsafe IntPtr* il2cpp_domain_get_assemblies(IntPtr domainPtr, out int size)
    {
        var domainHandle = GCHandle.FromIntPtr(domainPtr);
        var domain = (AppDomain)domainHandle.Target;
        
        var assemblies = domain.GetAssemblies();
        var assemblyPtrs = new IntPtr[assemblies.Length];
        
        for (var i = 0; i < assemblies.Length; i++)
        {
            var assemblyHandle = GCHandle.Alloc(assemblies[i]);
            assemblyPtrs[i] = GCHandle.ToIntPtr(assemblyHandle);
        }
        
        size = assemblyPtrs.Length;
        
        // 分配非托管内存并复制数组
        var ptr = (IntPtr*)Marshal.AllocHGlobal(IntPtr.Size * size);
        for (var i = 0; i < size; i++) ptr[i] = assemblyPtrs[i];
        
        return ptr;
    }
    
    // 模拟 il2cpp_assembly_get_image
    // 这里由于il2cpp_domain_get_assemblies返回的assemblies可以直接使用，所以在c层该函数只是一个抽象宏
    /*public static IntPtr il2cpp_assembly_get_image(IntPtr assemblyPtr)
    {
        return assemblyPtr;
    }*/
    
    // 模拟 il2cpp_get_corlib
    public static IntPtr il2cpp_get_corlib()
    {
        // 获取核心库 (mscorlib 或 System.Private.CoreLib)
        var corlibAssembly = typeof(object).Assembly;
        var corlibHandle = GCHandle.Alloc(corlibAssembly);
        return GCHandle.ToIntPtr(corlibHandle);
    }

    public static void il2cpp_free(IntPtr obj)
    {
        var handle = GCHandle.FromIntPtr(obj);
        if (handle.IsAllocated) handle.Free();
    }
    
    /// <summary>
    /// 复制对象（分配新内存）
    /// </summary>
    /// <param name="sourcePtr">源对象句柄</param>
    /// <returns>复制后的新对象句柄</returns>
    public static IntPtr il2cpp_object_copy(IntPtr sourcePtr)
    {
        if (sourcePtr == IntPtr.Zero)
            return IntPtr.Zero;
        
        var sourceHandle = GCHandle.FromIntPtr(sourcePtr);
        var sourceObj = sourceHandle.Target;
        
        if (sourceObj == null)
            return IntPtr.Zero;
        
        // 分配新的 GCHandle 指向同一个对象（浅拷贝）
        var copyHandle = GCHandle.Alloc(sourceObj);
        return GCHandle.ToIntPtr(copyHandle);
    }
}