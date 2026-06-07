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
        if (target == IntPtr.Zero)
        {
            Logger.Error("SetNativeValue: target pointer is NULL");
            return false;
        }

        if (value == null)
        {
            Logger.Error("SetNativeValue: value is NULL");
            return false;
        }

        try
        {
            var type = value.GetType();
            // ReSharper disable once InterpolatedStringExpressionIsNotIFormattable
            Logger.Debug($"SetNativeValue: writing value '{value}' of type '{type.Name}' to address {target:X8}");

            // 根据类型写入对应的值
            if (type == typeof(sbyte))
            {
                Marshal.WriteByte(target, (byte)(sbyte)value);
                Logger.Debug($"Wrote sbyte: {(sbyte)value}");
                return true;
            }

            if (type == typeof(byte))
            {
                Marshal.WriteByte(target, (byte)value);
                Logger.Debug($"Wrote byte: {(byte)value}");
                return true;
            }

            if (type == typeof(short))
            {
                Marshal.WriteInt16(target, (short)value);
                Logger.Debug($"Wrote short: {(short)value}");
                return true;
            }

            if (type == typeof(ushort))
            {
                Marshal.WriteInt16(target, (short)(ushort)value);
                Logger.Debug($"Wrote ushort: {(ushort)value}");
                return true;
            }

            if (type == typeof(int))
            {
                Marshal.WriteInt32(target, (int)value);
                Logger.Debug($"Wrote int: {(int)value}");
                return true;
            }

            if (type == typeof(uint))
            {
                Marshal.WriteInt32(target, (int)(uint)value);
                Logger.Debug($"Wrote uint: {(uint)value}");
                return true;
            }

            if (type == typeof(long))
            {
                Marshal.WriteInt64(target, (long)value);
                Logger.Debug($"Wrote long: {(long)value}");
                return true;
            }

            if (type == typeof(ulong))
            {
                Marshal.WriteInt64(target, (long)(ulong)value);
                Logger.Debug($"Wrote ulong: {(ulong)value}");
                return true;
            }

            if (type == typeof(float))
            {
                // float 需要特殊处理
                unsafe
                {
                    var floatVal = (float)value;
                    *(float*)target = floatVal;
                }

                Logger.Debug($"Wrote float: {(float)value}");
                return true;
            }

            if (type == typeof(double))
            {
                unsafe
                {
                    var doubleVal = (double)value;
                    *(double*)target = doubleVal;
                }

                Logger.Debug($"Wrote double: {(double)value}");
                return true;
            }

            if (type == typeof(bool))
            {
                Marshal.WriteByte(target, (bool)value ? (byte)1 : (byte)0);
                Logger.Debug($"Wrote bool: {(bool)value}");
                return true;
            }

            if (type == typeof(char))
            {
                Marshal.WriteInt16(target, (short)(char)value);
                Logger.Debug($"Wrote char: {(char)value}");
                return true;
            }

            if (type == typeof(IntPtr))
            {
                Marshal.WriteIntPtr(target, (IntPtr)value);
                // ReSharper disable once InterpolatedStringExpressionIsNotIFormattable
                Logger.Debug($"Wrote IntPtr: {(IntPtr)value:X8}");
                return true;
            }

            // 其他类型（引用类型）使用 GCHandle
            Logger.Debug($"Writing reference type: {type.Name}");
            var ptr = ObjectToPtr(value);
            Marshal.WriteIntPtr(target, ptr);
            // ReSharper disable once InterpolatedStringExpressionIsNotIFormattable
            Logger.Debug($"Wrote reference pointer: {ptr:X8}");
            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"SetNativeValue failed for type {value.GetType().Name}: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    ///     从非托管内存读取值
    /// </summary>
    public static object? GetNativeValue(IntPtr source, Type? targetType)
    {
        if (source == IntPtr.Zero)
        {
            Logger.Error("GetNativeValue: source pointer is NULL");
            return null;
        }

        if (targetType == null)
        {
            Logger.Error("GetNativeValue: target type is NULL");
            return null;
        }

        try
        {
            // ReSharper disable once InterpolatedStringExpressionIsNotIFormattable
            Logger.Debug($"GetNativeValue: reading from {source:X8} as type {targetType.Name}");

            // 基本类型读取
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

            // 引用类型：从 GCHandle 读取
            var objPtr = Marshal.ReadIntPtr(source);
            if (objPtr == IntPtr.Zero)
                return null;

            return PtrToObject(objPtr);
        }
        catch (Exception ex)
        {
            Logger.Error($"GetNativeValue failed for type {targetType.Name}: {ex.Message}");
            return null;
        }
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