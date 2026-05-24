// /*******************************************************************************
//  * tefloader - Field.cs
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

public static class Field
{
    // 模拟 il2cpp_field_get_name
    public static string? il2cpp_field_get_name(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return null;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        return field.Name;
    }

    // 模拟 il2cpp_field_get_parent
    public static IntPtr il2cpp_field_get_parent(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        var declaringType = field.DeclaringType;
        if (declaringType == null)
            return IntPtr.Zero;

        var parentHandle = GCHandle.Alloc(declaringType);
        return GCHandle.ToIntPtr(parentHandle);
    }

    // 模拟 il2cpp_field_get_offset
    /*public static int il2cpp_field_get_offset(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return -1;

        try
        {
            var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
            var field = (FieldInfo)fieldHandle.Target;

            if (field.DeclaringType != null)
            {
                var offset = Marshal.OffsetOf(field.DeclaringType, field.Name);
                return offset.ToInt32();
            }
        }
        catch
        {
            // 忽略错误
        }

        return -1;
    }*/

    // 模拟 il2cpp_field_get_type
    public static IntPtr il2cpp_field_get_type(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        var fieldType = field.FieldType;
        var typeHandle = GCHandle.Alloc(fieldType);
        return GCHandle.ToIntPtr(typeHandle);
    }

    // 额外功能：il2cpp_field_get_value - 获取实例字段的值（输出到指针）
    public static unsafe void il2cpp_field_get_value(IntPtr fieldPtr, IntPtr objPtr, IntPtr returnValuePtr)
    {
        if (fieldPtr == IntPtr.Zero || returnValuePtr == IntPtr.Zero)
            return;

        try
        {
            var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
            var field = (FieldInfo)fieldHandle.Target;

            object? target = null;
            if (objPtr != IntPtr.Zero)
                target = Utils.PtrToObject(objPtr);

            if (field.IsStatic)
                target = null;
            else if (target == null)
            {
                Logger.Error($"Instance field {field.Name} requires a valid object");
                return;
            }

            var value = field.GetValue(target);
            var fieldType = field.FieldType;

            // 根据字段类型输出值
            if (fieldType.IsValueType)
                // 值类型直接复制
                Utils.SetNativeValue(returnValuePtr, value);
            else
            {
                // 引用类型返回 GCHandle 指针
                if (value == null)
                    *(IntPtr*)returnValuePtr = IntPtr.Zero;
                else
                {
                    var resultPtr = Utils.ObjectToPtr(value);
                    *(IntPtr*)returnValuePtr = resultPtr;
                }
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get field value: {ex.Message}");
        }
    }

    // 额外功能：il2cpp_field_set_value - 设置实例字段的值
    public static bool il2cpp_field_set_value(IntPtr fieldPtr, IntPtr objPtr, IntPtr valuePtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return false;

        try
        {
            var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
            var field = (FieldInfo)fieldHandle.Target;

            object? target = null;
            if (objPtr != IntPtr.Zero)
                target = Utils.PtrToObject(objPtr);

            if (field.IsStatic)
            {
                target = null;
            }
            else if (target == null)
            {
                Logger.Error($"Instance field {field.Name} requires a valid object");
                return false;
            }

            object? value = null;
            if (valuePtr != IntPtr.Zero)
                value = Utils.PtrToObject(valuePtr);

            field.SetValue(target, value);
            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to set field value: {ex.Message}");
            return false;
        }
    }

    // 额外功能：il2cpp_field_is_static - 判断字段是否为静态
    public static bool il2cpp_field_is_static(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return false;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        return field.IsStatic;
    }

    // 额外功能：il2cpp_field_is_thread_static - 判断是否为线程静态字段
    public static bool il2cpp_field_is_thread_static(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return false;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        // 检查是否有 ThreadStaticAttribute
        var threadStaticAttr = field.GetCustomAttribute<ThreadStaticAttribute>();
        return threadStaticAttr != null;
    }


// 额外功能：il2cpp_field_is_literal - 判断字段是否为常量
    public static bool il2cpp_field_is_literal(IntPtr fieldPtr)
    {
        if (fieldPtr == IntPtr.Zero)
            return false;

        var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
        var field = (FieldInfo)fieldHandle.Target;

        return field.IsLiteral;
    }
}