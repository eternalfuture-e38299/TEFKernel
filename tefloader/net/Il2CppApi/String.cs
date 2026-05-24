// /*******************************************************************************
//  * tefloader - String.cs
//  * Copyright (C) 2026 eternalfuture-e38299
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU Affero General Public License as published by
//  * the Free Software Foundation, either version 3 of the License, or
//  * (at your option) any later version.
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  * GNU Affero General Public License for more details.
//  *
//  * You should have received a copy of the GNU Affero General Public License
//  * along with this program. If not, see <https://www.gnu.org/licenses/>.
//  *
//  * Author: eternalfuture-e38299
//  * GitHub: https://github.com/eternalfuture-e38299
//  * Created: $[InvalidReference]
//  *******************************************************************************/

using System.Runtime.InteropServices;

namespace tefloader.Il2CppApi;

public class String
{
    // 模拟 il2cpp_string_length - 获取字符串长度
    public static int il2cpp_string_length(IntPtr strPtr)
    {
        if (strPtr == IntPtr.Zero)
            return 0;
        
        var strHandle = GCHandle.FromIntPtr(strPtr);
        var str = (string)strHandle.Target;
        
        return str.Length;
    }
    
    // 模拟 il2cpp_string_chars - 获取字符指针
    /*public static unsafe char* il2cpp_string_chars(IntPtr strPtr)
    {
        if (strPtr == IntPtr.Zero)
            return null;
        
        var strHandle = GCHandle.FromIntPtr(strPtr);
        var str = (string)strHandle.Target;
        
        // 固定字符串获取字符指针
        fixed (char* ptr = str)
        {
            return ptr;
        }
    }*/
    
    // 模拟 il2cpp_string_new - 创建新字符串
    public static unsafe IntPtr il2cpp_string_new(byte* cstrPtr)
    {
        if (cstrPtr == null)
            return IntPtr.Zero;
        
        // 计算字符串长度
        var length = 0;
        while (cstrPtr[length] != 0)
            length++;
        
        // 转换为托管字符串
        var bytes = new byte[length];
        Marshal.Copy((IntPtr)cstrPtr, bytes, 0, length);
        
        var str = Encoding.UTF8.GetString(bytes);
        
        var strHandle = GCHandle.Alloc(str);
        return GCHandle.ToIntPtr(strHandle);
    }
    
    // 额外功能：il2cpp_string_cstr - 获取 C 风格字符串（非托管分配）
    public static unsafe byte* il2cpp_string_cstr(IntPtr strPtr)
    {
        if (strPtr == IntPtr.Zero)
            return null;
        
        var strHandle = GCHandle.FromIntPtr(strPtr);
        var str = (string)strHandle.Target;
        
        if (string.IsNullOrEmpty(str))
            return null;
        
        // 将字符串转换为 UTF-8 字节数组
        var utf8Bytes = Encoding.UTF8.GetBytes(str);
        
        // 分配非托管内存（包括结尾的 '\0'）
        var size = utf8Bytes.Length + 1;
        var ptr = Marshal.AllocHGlobal(size);
        
        // 复制字节数据
        Marshal.Copy(utf8Bytes, 0, ptr, utf8Bytes.Length);
        
        // 添加结尾的 '\0'
        Marshal.WriteByte(ptr, utf8Bytes.Length, 0);
        
        return (byte*)ptr;
    }
}