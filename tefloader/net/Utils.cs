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

        var handle = GCHandle.Alloc(obj);
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
    ///     将值类型复制到非托管内存
    /// </summary>
    public static bool SetNativeValue(IntPtr target, object? value)
    {
        if (target == IntPtr.Zero || value == null)
            return false;

        try
        {
            // 根据值的类型进行处理
            switch (value)
            {
                // 基本整数类型
                case sbyte sbyteVal:
                    Marshal.WriteByte(target, (byte)sbyteVal);
                    return true;

                case byte byteVal:
                    Marshal.WriteByte(target, byteVal);
                    return true;

                case short shortVal:
                    Marshal.WriteInt16(target, shortVal);
                    return true;

                case ushort ushortVal:
                    Marshal.WriteInt16(target, (short)ushortVal);
                    return true;

                case int intVal:
                    Marshal.WriteInt32(target, intVal);
                    return true;

                case uint uintVal:
                    Marshal.WriteInt32(target, (int)uintVal);
                    return true;

                case long longVal:
                    Marshal.WriteInt64(target, longVal);
                    return true;

                case ulong ulongVal:
                    Marshal.WriteInt64(target, (long)ulongVal);
                    return true;

                // 浮点类型
                case float floatVal:
                    unsafe
                    {
                        *(float*)target = floatVal;
                    }

                    return true;

                case double doubleVal:
                    unsafe
                    {
                        *(double*)target = doubleVal;
                    }

                    return true;

                // 布尔类型
                case bool boolVal:
                    Marshal.WriteByte(target, boolVal ? (byte)1 : (byte)0);
                    return true;

                // 字符类型
                case char charVal:
                    Marshal.WriteInt16(target, (short)charVal);
                    return true;

                // 指针类型
                case IntPtr ptrVal:
                    Marshal.WriteIntPtr(target, ptrVal);
                    return true;

                // 引用类型 - 使用 GCHandle
                default:
                    var ptr = ObjectToPtr(value);
                    Marshal.WriteIntPtr(target, ptr);
                    return true;
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"SetNativeValue error: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    ///     从非托管内存读取值
    /// </summary>
    public static object? GetNativeValue(IntPtr source, Type targetType)
    {
        if (source == IntPtr.Zero)
            return null;

        try
        {
            // 基本类型处理
            if (targetType == typeof(sbyte))
                return (sbyte)Marshal.ReadByte(source);

            if (targetType == typeof(byte))
                return Marshal.ReadByte(source);

            if (targetType == typeof(short))
                return Marshal.ReadInt16(source);

            if (targetType == typeof(ushort))
                return (ushort)Marshal.ReadInt16(source);

            if (targetType == typeof(int))
                return Marshal.ReadInt32(source);

            if (targetType == typeof(uint))
                return (uint)Marshal.ReadInt32(source);

            if (targetType == typeof(long))
                return Marshal.ReadInt64(source);

            if (targetType == typeof(ulong))
                return (ulong)Marshal.ReadInt64(source);

            if (targetType == typeof(float))
                unsafe
                {
                    return *(float*)source;
                }

            if (targetType == typeof(double))
                unsafe
                {
                    return *(double*)source;
                }

            if (targetType == typeof(bool))
                return Marshal.ReadByte(source) != 0;

            if (targetType == typeof(char))
                return (char)Marshal.ReadInt16(source);

            if (targetType == typeof(IntPtr))
                return Marshal.ReadIntPtr(source);

            // 引用类型 - 从 GCHandle 获取
            var ptr = Marshal.ReadIntPtr(source);
            return PtrToObject(ptr);
        }
        catch (Exception ex)
        {
            Logger.Error($"GetNativeValue error: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    ///     获取类型对应的 PatchType 枚举值
    /// </summary>
    public static int GetPatchTypeByType(Type type)
    {
        if (type == typeof(void))
            return (int)NetApi.Type.PatchType.PatchVoid;
        if (type == typeof(sbyte))
            return (int)NetApi.Type.PatchType.PatchInt8;
        if (type == typeof(short))
            return (int)NetApi.Type.PatchType.PatchInt16;
        if (type == typeof(int))
            return (int)NetApi.Type.PatchType.PatchInt32;
        if (type == typeof(long))
            return (int)NetApi.Type.PatchType.PatchInt64;
        if (type == typeof(byte))
            return (int)NetApi.Type.PatchType.PatchUint8;
        if (type == typeof(ushort))
            return (int)NetApi.Type.PatchType.PatchUint16;
        if (type == typeof(uint))
            return (int)NetApi.Type.PatchType.PatchUint32;
        if (type == typeof(ulong))
            return (int)NetApi.Type.PatchType.PatchUint64;
        if (type == typeof(bool))
            return (int)NetApi.Type.PatchType.PatchBool;
        if (type == typeof(float))
            return (int)NetApi.Type.PatchType.PatchFloat;
        if (type == typeof(double))
            return (int)NetApi.Type.PatchType.PatchDouble;
        if (type == typeof(char))
            return (int)NetApi.Type.PatchType.PatchChar;
        if (type.IsPointer)
            return (int)NetApi.Type.PatchType.PatchPointer;

        return (int)NetApi.Type.PatchType.PatchObject;
    }

    /// <summary>
    ///     判断类型是否为值类型（基础类型）
    /// </summary>
    public static bool IsValueType(Type type)
    {
        return type.IsValueType || type == typeof(void) || type.IsPrimitive;
    }

    /// <summary>
    ///     获取类型的大小（用于值类型）
    /// </summary>
    public static int GetTypeSize(Type type)
    {
        if (type == typeof(void)) return 0;
        if (type == typeof(sbyte) || type == typeof(byte)) return 1;
        if (type == typeof(short) || type == typeof(ushort)) return 2;
        if (type == typeof(int) || type == typeof(uint) || type == typeof(float)) return 4;
        if (type == typeof(long) || type == typeof(ulong) || type == typeof(double)) return 8;
        if (type == typeof(bool)) return 1;
        if (type == typeof(char)) return 2;
        if (type == typeof(IntPtr)) return IntPtr.Size;

        return Marshal.SizeOf(type);
    }
}