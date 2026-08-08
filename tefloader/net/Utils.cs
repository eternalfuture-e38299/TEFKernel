/*******************************************************************************
 * tefkernel - Utils.cs
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
 * Created: 2026/01/03
 *******************************************************************************/

using System.Runtime.InteropServices;

namespace tefloader;

public static class Utils
{
    public static T[] CArrayToNetArray<T>(IntPtr array, int length)
    {
        if (array == IntPtr.Zero || length <= 0)
            return [];

        var result = new T[length];

        var elementSize = Marshal.SizeOf(typeof(T));
        var totalBytes = length * elementSize;

        var buffer = new byte[totalBytes];
        Marshal.Copy(array, buffer, 0, totalBytes);

        Buffer.BlockCopy(buffer, 0, result, 0, totalBytes);

        return result;
    }

    /// <summary>
    ///     将托管对象转换为指针（使用 GCHandle）
    /// </summary>
    public static IntPtr ObjectToPtr(object? obj)
    {
        if (obj == null)
            return IntPtr.Zero;

        var handle = GCHandle.Alloc(obj, GCHandleType.Normal);
        return GCHandle.ToIntPtr(handle);
    }

    /// <summary>
    ///     将指针转换为托管对象（从 GCHandle 解引用）
    /// </summary>
    public static object? PtrToObject(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
            return null;

        var handle = GCHandle.FromIntPtr(ptr);
        return handle.Target;
    }

    /// <summary>
    ///     将值复制到非托管内存
    /// </summary>
    public static bool SetNativeValue(IntPtr target, object? value)
    {
        if (target == IntPtr.Zero || value == null)
            return false;

        try
        {
            var type = value.GetType();

            // 处理枚举类型
            if (type.IsEnum)
            {
                return WriteEnumValue(target, value);
            }

            if (type == typeof(sbyte))
            {
                Marshal.WriteByte(target, (byte)(sbyte)value);
                return true;
            }

            if (type == typeof(byte))
            {
                Marshal.WriteByte(target, (byte)value);
                return true;
            }

            if (type == typeof(short))
            {
                Marshal.WriteInt16(target, (short)value);
                return true;
            }

            if (type == typeof(ushort))
            {
                Marshal.WriteInt16(target, (short)(ushort)value);
                return true;
            }

            if (type == typeof(int))
            {
                Marshal.WriteInt32(target, (int)value);
                return true;
            }

            if (type == typeof(uint))
            {
                Marshal.WriteInt32(target, (int)(uint)value);
                return true;
            }

            if (type == typeof(long))
            {
                Marshal.WriteInt64(target, (long)value);
                return true;
            }

            if (type == typeof(ulong))
            {
                Marshal.WriteInt64(target, (long)(ulong)value);
                return true;
            }

            if (type == typeof(float))
            {
                unsafe
                {
                    var floatVal = (float)value;
                    *(float*)target = floatVal;
                }
                return true;
            }

            if (type == typeof(double))
            {
                unsafe
                {
                    var doubleVal = (double)value;
                    *(double*)target = doubleVal;
                }
                return true;
            }

            if (type == typeof(bool))
            {
                Marshal.WriteByte(target, (bool)value ? (byte)1 : (byte)0);
                return true;
            }

            if (type == typeof(char))
            {
                Marshal.WriteInt16(target, (short)(char)value);
                return true;
            }

            if (type == typeof(IntPtr) || type == typeof(UIntPtr))
            {
                Marshal.WriteIntPtr(target, (IntPtr)value);
                return true;
            }

            // 引用类型：使用 GCHandle
            var ptr = ObjectToPtr(value);
            Marshal.WriteIntPtr(target, ptr);
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    ///     将枚举值写入非托管内存
    /// </summary>
    private static bool WriteEnumValue(IntPtr target, object? enumValue)
    {
        if (target == IntPtr.Zero || enumValue == null || !enumValue.GetType().IsEnum)
            return false;

        Type enumType = enumValue.GetType();
        Type underlyingType = Enum.GetUnderlyingType(enumType);

        // 获取枚举的原始值
        object rawValue = Convert.ChangeType(enumValue, underlyingType);

        // 根据基础类型写入
        if (underlyingType == typeof(sbyte) || underlyingType == typeof(byte))
        {
            Marshal.WriteByte(target, Convert.ToByte(rawValue));
            return true;
        }

        if (underlyingType == typeof(short) || underlyingType == typeof(ushort))
        {
            Marshal.WriteInt16(target, Convert.ToInt16(rawValue));
            return true;
        }

        if (underlyingType == typeof(int) || underlyingType == typeof(uint))
        {
            Marshal.WriteInt32(target, Convert.ToInt32(rawValue));
            return true;
        }

        if (underlyingType == typeof(long) || underlyingType == typeof(ulong))
        {
            Marshal.WriteInt64(target, Convert.ToInt64(rawValue));
            return true;
        }

        // 默认按 int 处理
        Marshal.WriteInt32(target, Convert.ToInt32(rawValue));
        return true;
    }

    /// <summary>
    ///     从非托管内存读取值
    /// </summary>
    public static object? GetNativeValue(IntPtr source, Type? targetType)
    {
        if (source == IntPtr.Zero || targetType == null)
            return null;

        try
        {
            // 处理枚举类型
            if (targetType.IsEnum)
            {
                return ReadEnumValue(source, targetType);
            }

            // 基本类型读取（对应 patch_type_t）
            if (targetType == typeof(void)) return null;
            if (targetType == typeof(sbyte)) return (sbyte)Marshal.ReadByte(source);
            if (targetType == typeof(byte)) return Marshal.ReadByte(source);
            if (targetType == typeof(short)) return Marshal.ReadInt16(source);
            if (targetType == typeof(ushort)) return (ushort)Marshal.ReadInt16(source);
            if (targetType == typeof(int)) return Marshal.ReadInt32(source);
            if (targetType == typeof(uint)) return (uint)Marshal.ReadInt32(source);
            if (targetType == typeof(long)) return Marshal.ReadInt64(source);
            if (targetType == typeof(ulong)) return (ulong)Marshal.ReadInt64(source);
            if (targetType == typeof(bool)) return Marshal.ReadByte(source) != 0;
            if (targetType == typeof(float)) unsafe { return *(float*)source; }
            if (targetType == typeof(double)) unsafe { return *(double*)source; }
            if (targetType == typeof(IntPtr)) return Marshal.ReadIntPtr(source);
            if (targetType == typeof(UIntPtr)) return (UIntPtr)Marshal.ReadIntPtr(source).ToInt64();
            if (targetType == typeof(char)) return (char)Marshal.ReadInt16(source);

            // PATCH_OBJECT: 从指针读取 GCHandle
            var ptr = Marshal.ReadIntPtr(source);
            if (ptr == IntPtr.Zero) return null;

            var handle = GCHandle.FromIntPtr(ptr);
            return handle.IsAllocated ? handle.Target : null;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    ///     从非托管内存读取枚举值
    /// </summary>
    private static object? ReadEnumValue(IntPtr source, Type enumType)
    {
        if (source == IntPtr.Zero || !enumType.IsEnum)
            return null;

        // 获取枚举的基础类型
        var underlyingType = Enum.GetUnderlyingType(enumType);

        // 根据基础类型读取值
        object rawValue = underlyingType switch
        {
            not null when underlyingType == typeof(sbyte) => (sbyte)Marshal.ReadByte(source),
            not null when underlyingType == typeof(byte) => Marshal.ReadByte(source),
            not null when underlyingType == typeof(short) => Marshal.ReadInt16(source),
            not null when underlyingType == typeof(ushort) => (ushort)Marshal.ReadInt16(source),
            not null when underlyingType == typeof(int) => Marshal.ReadInt32(source),
            not null when underlyingType == typeof(uint) => (uint)Marshal.ReadInt32(source),
            not null when underlyingType == typeof(long) => Marshal.ReadInt64(source),
            not null when underlyingType == typeof(ulong) => (ulong)Marshal.ReadInt64(source),
            _ => Marshal.ReadInt32(source) // 默认按 int 处理
        };

        // 将原始值转换为枚举
        return Enum.ToObject(enumType, rawValue);
    }

    /// <summary>
    /// 判断类型是否为基本类型（对应 C 端的 patch_type_t）
    /// 只有这些类型被视为基础类型，其他全部视为 PATCH_OBJECT
    /// </summary>
    public static bool IsValueType(Type? type)
    {
        if (type == null) return false;

        // 枚举是值类型
        if (type.IsEnum) return true;

        // 对应 PATCH_VOID
        if (type == typeof(void)) return true;

        // 对应 PATCH_INT8, PATCH_UINT8
        if (type == typeof(sbyte) || type == typeof(byte)) return true;

        // 对应 PATCH_INT16, PATCH_UINT16
        if (type == typeof(short) || type == typeof(ushort)) return true;

        // 对应 PATCH_INT32, PATCH_UINT32
        if (type == typeof(int) || type == typeof(uint)) return true;

        // 对应 PATCH_INT64, PATCH_UINT64
        if (type == typeof(long) || type == typeof(ulong)) return true;

        // 对应 PATCH_BOOL
        if (type == typeof(bool)) return true;

        // 对应 PATCH_FLOAT
        if (type == typeof(float)) return true;

        // 对应 PATCH_DOUBLE
        if (type == typeof(double)) return true;

        // 对应 PATCH_POINTER
        if (type == typeof(IntPtr) || type == typeof(UIntPtr)) return true;

        // 对应 PATCH_CHAR
        if (type == typeof(char)) return true;

        // 所有其他类型（包括 string、数组、类、结构体等）都视为 PATCH_OBJECT
        return false;
    }

    /// <summary>
    ///     获取类型的大小（用于值类型）
    /// </summary>
    public static int GetTypeSize(Type? type)
    {
        if (type == null) return 0;

        // 枚举类型：返回其基础类型的大小
        if (type.IsEnum)
        {
            Type underlyingType = Enum.GetUnderlyingType(type);
            return GetTypeSize(underlyingType);
        }

        if (type == typeof(void)) return 0;
        if (type == typeof(sbyte) || type == typeof(byte)) return 1;
        if (type == typeof(short) || type == typeof(ushort)) return 2;
        if (type == typeof(int) || type == typeof(uint) || type == typeof(float)) return 4;
        if (type == typeof(long) || type == typeof(ulong) || type == typeof(double)) return 8;
        if (type == typeof(bool)) return 1;
        if (type == typeof(char)) return 2;
        if (type == typeof(IntPtr) || type == typeof(UIntPtr)) return IntPtr.Size;

        // 结构体或类：使用 Marshal.SizeOf 或按指针处理
        try
        {
            return Marshal.SizeOf(type);
        }
        catch
        {
            // 如果无法获取大小，按指针处理
            return IntPtr.Size;
        }
    }

    /// <summary>
    ///     释放 GCHandle 指针
    /// </summary>
    public static void FreeGCHandle(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
            return;

        try
        {
            var handle = GCHandle.FromIntPtr(ptr);
            if (handle.IsAllocated)
                handle.Free();
        }
        catch
        {
            // 忽略无效的 GCHandle
        }
    }

    /// <summary>
    ///     检查指针是否为有效的 GCHandle
    /// </summary>
    public static bool IsValidGCHandle(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
            return false;

        try
        {
            var handle = GCHandle.FromIntPtr(ptr);
            return handle.IsAllocated;
        }
        catch
        {
            return false;
        }
    }
}