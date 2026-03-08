// /*******************************************************************************
//  * tefkernel - Utils.cs
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
//  * Created: 2026/01/03
//  *******************************************************************************/

using System.Runtime.InteropServices;
using tefloader.NetApi;
using Type = System.Type;

namespace tefloader;

public static class Utils
{
    public static T[] CArrayToNetArray<T>(IntPtr array, int length)
    {
        if (array == IntPtr.Zero || length <= 0)
            return [];
    
        var result = new T[length];
    
        // 使用 Buffer.BlockCopy 兼容方法
        var elementSize = Marshal.SizeOf(typeof(T));
        var totalBytes = length * elementSize;
    
        // 创建字节数组作为缓冲区
        var buffer = new byte[totalBytes];
        Marshal.Copy(array, buffer, 0, totalBytes);
    
        // 从字节数组转换
        Buffer.BlockCopy(buffer, 0, result, 0, totalBytes);
    
        return result;
    }
    
    public static bool SetNativeValue(IntPtr target, object value)
    {
        if (target == IntPtr.Zero)
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
                    var floatBytes = BitConverter.GetBytes(floatVal);
                    Marshal.Copy(floatBytes, 0, target, 4);
                    return true;
                    
                case double doubleVal:
                    var doubleBytes = BitConverter.GetBytes(doubleVal);
                    Marshal.Copy(doubleBytes, 0, target, 8);
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
                
                // 引用类型 - 存储到Assets.Objects中
                default:
                    // 将对象添加到Assets.Objects中，返回句柄
                    var handle = Asset.Objects.Add(value);
                    Marshal.WriteInt32(target, handle);
                    return true;
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"SetNativeValue error: {ex.Message}");
            return false;
        }
    }
    
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
            {
                var bytes = new byte[4];
                Marshal.Copy(source, bytes, 0, 4);
                return BitConverter.ToSingle(bytes, 0);
            }
                
            if (targetType == typeof(double))
            {
                var bytes = new byte[8];
                Marshal.Copy(source, bytes, 0, 8);
                return BitConverter.ToDouble(bytes, 0);
            }
                
            if (targetType == typeof(bool))
                return Marshal.ReadByte(source) != 0;
                
            if (targetType == typeof(char))
                return (char)Marshal.ReadInt16(source);
                
            if (targetType == typeof(IntPtr))
                return Marshal.ReadIntPtr(source);
            
            // 引用类型 - 从Assets.Objects中获取
            var handle = Marshal.ReadInt32(source);
            if (handle >= 0 && handle < Asset.Objects.Count)
                return Asset.Objects[handle];
            
            return null;
        }
        catch (Exception ex)
        {
            Logger.Error($"GetNativeValue error: {ex.Message}");
            return null;
        }
    }
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
}