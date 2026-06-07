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

    // 额外：il2cpp_field_get_value
    public static unsafe void il2cpp_field_get_value(IntPtr fieldPtr, IntPtr objPtr, IntPtr returnValuePtr)
    {
        if (fieldPtr == IntPtr.Zero || returnValuePtr == IntPtr.Zero)
        {
            Logger.Error("il2cpp_field_get_value: Invalid parameters");
            return;
        }

        try
        {
            var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
            var field = (FieldInfo)fieldHandle.Target;

            if (field == null)
            {
                Logger.Error("il2cpp_field_get_value: Invalid field");
                return;
            }

            object? target;

            if (field.IsStatic)
            {
                target = null;
            }
            else
            {
                if (objPtr == IntPtr.Zero)
                {
                    Logger.Error($"Instance field '{field.Name}' requires a valid object");
                    return;
                }

                target = Utils.PtrToObject(objPtr);
                if (target == null)
                {
                    Logger.Error($"Failed to get object from pointer for field '{field.Name}'");
                    return;
                }
            }

            // 获取字段值
            var value = field.GetValue(target);
            var fieldType = field.FieldType;

            if (value == null)
            {
                // 写入默认值
                if (fieldType.IsValueType)
                {
                    var size = Utils.GetTypeSize(fieldType);
                    for (var i = 0; i < size; i++) ((byte*)returnValuePtr)[i] = 0;
                }
                else
                {
                    *(IntPtr*)returnValuePtr = IntPtr.Zero;
                }
            }
            else if (fieldType.IsValueType)
            {
                // 使用 Utils.SetNativeValue 写入值
                Utils.SetNativeValue(returnValuePtr, value);
            }
            else
            {
                // 引用类型：存储 GCHandle 指针
                var ptr = Utils.ObjectToPtr(value);
                *(IntPtr*)returnValuePtr = ptr;
            }

            Logger.Debug($"Field '{field.Name}' read successfully");
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get field value: {ex.Message}");
        }
    }

    // 额外：il2cpp_field_set_value
    public static unsafe bool il2cpp_field_set_value(IntPtr fieldPtr, IntPtr objPtr, IntPtr valuePtr)
    {
        if (fieldPtr == IntPtr.Zero)
        {
            Logger.Error("il2cpp_field_set_value: fieldPtr is zero");
            return false;
        }

        try
        {
            var fieldHandle = GCHandle.FromIntPtr(fieldPtr);
            var field = (FieldInfo)fieldHandle.Target;

            if (field == null)
            {
                Logger.Error("il2cpp_field_set_value: Invalid field");
                return false;
            }

            object? target;

            if (field.IsStatic)
            {
                target = null;
            }
            else
            {
                if (objPtr == IntPtr.Zero)
                {
                    Logger.Error($"Instance field '{field.Name}' requires a valid object");
                    return false;
                }

                target = Utils.PtrToObject(objPtr);
                if (target == null)
                {
                    Logger.Error($"Failed to get object from pointer for field '{field.Name}'");
                    return false;
                }
            }

            var fieldType = field.FieldType;
            object? value = null;

            if (fieldType.IsValueType)
            {
                // 使用 Utils.GetNativeValue 读取值
                value = Utils.GetNativeValue(valuePtr, fieldType) ?? Activator.CreateInstance(fieldType);
            }
            else
            {
                // 引用类型：从 GCHandle 指针获取对象
                if (valuePtr != IntPtr.Zero)
                {
                    var objHandlePtr = *(IntPtr*)valuePtr;
                    if (objHandlePtr != IntPtr.Zero) value = Utils.PtrToObject(objHandlePtr);
                }
            }

            field.SetValue(target, value);
            Logger.Debug($"Field '{field.Name}' set to: {value ?? "null"}");
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