// /*******************************************************************************
//  * tefloader - Property.cs
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

using System.Reflection;
using System.Runtime.InteropServices;

namespace tefloader.Il2CppApi;

public class Property
{
    // 模拟 il2cpp_property_get_name
    public static string? il2cpp_property_get_name(IntPtr propertyPtr)
    {
        if (propertyPtr == IntPtr.Zero)
            return null;
        
        var propertyHandle = GCHandle.FromIntPtr(propertyPtr);
        var property = (PropertyInfo)propertyHandle.Target;
        
        return property.Name;
    }
    
    // 模拟 il2cpp_property_get_get_method
    public static IntPtr il2cpp_property_get_get_method(IntPtr propertyPtr)
    {
        if (propertyPtr == IntPtr.Zero)
            return IntPtr.Zero;
        
        var propertyHandle = GCHandle.FromIntPtr(propertyPtr);
        var property = (PropertyInfo)propertyHandle.Target;
        
        var getMethod = property.GetGetMethod(true); // true 表示包括非公共方法
        if (getMethod == null)
            return IntPtr.Zero;
        
        var methodHandle = GCHandle.Alloc(getMethod);
        return GCHandle.ToIntPtr(methodHandle);
    }
    
    // 模拟 il2cpp_property_get_set_method
    public static IntPtr il2cpp_property_get_set_method(IntPtr propertyPtr)
    {
        if (propertyPtr == IntPtr.Zero)
            return IntPtr.Zero;
        
        var propertyHandle = GCHandle.FromIntPtr(propertyPtr);
        var property = (PropertyInfo)propertyHandle.Target;
        
        var setMethod = property.GetSetMethod(true); // true 表示包括非公共方法
        if (setMethod == null)
            return IntPtr.Zero;
        
        var methodHandle = GCHandle.Alloc(setMethod);
        return GCHandle.ToIntPtr(methodHandle);
    }
}