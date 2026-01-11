/*******************************************************************************
 * tefkernel - Type.cs
 * Copyright (C) 2025 eternalfuture-e38299
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
 * Created: 2025/11/23
 *******************************************************************************/

using System.Reflection;
using System.Runtime.InteropServices;

namespace tefloader.NetApi;

public static class Type
{
    public enum PatchType {
        PatchVoid,
        PatchInt8,
        PatchInt16,
        PatchInt32,
        PatchInt64,
        PatchUint8,
        PatchUint16,
        PatchUint32,
        PatchUint64,
        PatchBool,
        PatchFloat,
        PatchDouble,
        PatchPointer,
        PatchObject,
        PatchChar
    };
    
    public static readonly Dictionary<PatchType, System.Type> TypeMapping = new()
    {
        { PatchType.PatchVoid, typeof(void) },
        { PatchType.PatchInt8, typeof(sbyte) },
        { PatchType.PatchInt16, typeof(short) },
        { PatchType.PatchInt32, typeof(int) },
        { PatchType.PatchInt64, typeof(long) },
        { PatchType.PatchUint8, typeof(byte) },
        { PatchType.PatchUint16, typeof(ushort) },
        { PatchType.PatchUint32, typeof(uint) },
        { PatchType.PatchUint64, typeof(ulong) },
        { PatchType.PatchBool, typeof(bool) },
        { PatchType.PatchFloat, typeof(float) },
        { PatchType.PatchDouble, typeof(double) },
        { PatchType.PatchPointer, typeof(IntPtr) },
        { PatchType.PatchObject, typeof(object) },
        { PatchType.PatchChar, typeof(char) }
    };
    
    /// <summary>
    /// 获取类型句柄
    /// </summary>
    /// <param name="ns">命名空间</param>
    /// <param name="name">类型名称</param>
    /// <returns>类型句柄，失败返回 -1</returns>
    public static int GetType(string ns, string name)
    {
        var fullName = string.IsNullOrEmpty(ns) ? name : $"{ns}.{name}";
        foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
        {
            var type = assembly.GetType(fullName);
            if (type != null) return Asset.Types.Add(type);
        }

        return -1;
    }

    /// <summary>
    /// 创建类型的实例
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>实例句柄，失败返回 -1</returns>
    public static int NewInstance(int typeHandle)
    {
        if (typeHandle < 0) return -1;
        
        var type = Asset.Types[typeHandle];

        try
        {
            var instance = Activator.CreateInstance(type);
            return Asset.Objects.Add(instance);
        }
        catch
        {
            return -1;
        }
    }

    /// <summary>
    /// 构造泛型类型
    /// </summary>
    /// <param name="typeHandle">泛型类型定义句柄</param>
    /// <param name="genericTypes">泛型类型参数数组</param>
    /// <param name="typesSize">数组大小</param>
    /// <returns>泛型类型句柄，失败返回 -1</returns>
    public static int TypeMakeGeneric(int typeHandle, IntPtr genericTypes, int typesSize)
    {
        var genericTypesArray = Utils.CArrayToNetArray<int>(genericTypes, typesSize);
        
        if (typeHandle < 0 || genericTypes == IntPtr.Zero || typesSize <= 0) 
            return -1;
        
        var genericTypeDef = Asset.Types[typeHandle];
        if (!genericTypeDef.IsGenericTypeDefinition) 
            return -1;
        
        var typeArgs = new System.Type[typesSize];
        for (int i = 0; i < typesSize; i++)
        {
            var argType = Asset.Types[genericTypesArray[i]];
            typeArgs[i] = argType;
        }
        
        try
        {
            var genericType = genericTypeDef.MakeGenericType(typeArgs);
            return Asset.Types.Add(genericType);
        }
        catch
        {
            return -1;
        }
    }

    /// <summary>
    /// 获取类型名称
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>类型名称字符串，失败返回 null</returns>
    public static string TypeGetName(int typeHandle)
    {
        if (typeHandle < 0) return string.Empty;
        
        var type = Asset.Types[typeHandle];

        return type.Name;
    }

    /// <summary>
    /// 获取类型命名空间
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>命名空间字符串，失败返回 null</returns>
    public static IntPtr TypeGetNamespace(int typeHandle)
    {
        if (typeHandle < 0) return IntPtr.Zero;
        
        var type = Asset.Types[typeHandle];

        var ns = type.Namespace ?? string.Empty;
        return Marshal.StringToHGlobalAnsi(ns);
    }

    /// <summary>
    /// 获取父类型
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>父类型句柄，失败返回 -1</returns>
    public static int TypeGetParent(int typeHandle)
    {
        if (typeHandle < 0) return -1;
        
        var type = Asset.Types[typeHandle];

        var baseType = type.BaseType;
        if (baseType == null) return -1;
        
        return Asset.Types.Add(baseType);
    }

    /// <summary>
    /// 获取字段
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="name">字段名称</param>
    /// <returns>字段句柄，失败返回 -1</returns>
    public static int TypeGetField(int typeHandle, string name)
    {
        if (typeHandle < 0 || string.IsNullOrEmpty(name)) 
            return -1;
        
        var type = Asset.Types[typeHandle];

        var field = type.GetField(name, 
            BindingFlags.Public | BindingFlags.NonPublic | 
            BindingFlags.Instance | BindingFlags.Static);
        
        if (field == null) return -1;
        
        return Asset.FieldInfos.Add(field);
    }

    /// <summary>
    /// 获取属性
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="name">属性名称</param>
    /// <returns>属性句柄，失败返回 -1</returns>
    public static int TypeGetProperty(int typeHandle, string name)
    {
        if (typeHandle < 0 || string.IsNullOrEmpty(name)) 
            return -1;
        
        var type = Asset.Types[typeHandle];

        var property = type.GetProperty(name, 
            BindingFlags.Public | BindingFlags.NonPublic | 
            BindingFlags.Instance | BindingFlags.Static);
        
        if (property == null) return -1;
        
        return Asset.PropertyInfos.Add(property);
    }

    /// <summary>
    /// 获取函数（通过名称与参数数量）
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="name">方法名称</param>
    /// <param name="argsCount">参数数量</param>
    /// <returns>方法句柄，失败返回 -1</returns>
    public static int TypeGetMethodFromArgsCount(int typeHandle, string name, int argsCount)
    {
        if (typeHandle < 0 || string.IsNullOrEmpty(name) || argsCount < 0) 
            return -1;
        
        var type = Asset.Types[typeHandle];

        // 获取所有方法
        var methods = type.GetMethods(
            BindingFlags.Public | BindingFlags.NonPublic | 
            BindingFlags.Instance | BindingFlags.Static)
            .Where(m => m.Name == name)
            .ToArray();
        
        // 查找参数数量匹配的方法
        foreach (var method in methods)
        {
            var parameters = method.GetParameters();
            if (parameters.Length == argsCount)
            {
                return Asset.MethodInfos.Add(method);
            }
        }
        
        return -1;
    }
    
    /// <summary>
    /// 将类型转换为 patch_type_t
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>patch_type_t 枚举值</returns>
    public static int TypeToPatchlibType(int typeHandle)
    {
        if (typeHandle < 0) return 0; // PATCH_NULL
        
        var type = Asset.Types[typeHandle];

        // 映射 .NET 类型到 patch_type_t
        if (type == typeof(void)) return 1;        // PATCH_VOID
        if (type == typeof(sbyte)) return 2;       // PATCH_INT8
        if (type == typeof(short)) return 3;       // PATCH_INT16
        if (type == typeof(int)) return 4;         // PATCH_INT32
        if (type == typeof(long)) return 5;        // PATCH_INT64
        if (type == typeof(byte)) return 6;        // PATCH_UINT8
        if (type == typeof(ushort)) return 7;      // PATCH_UINT16
        if (type == typeof(uint)) return 8;        // PATCH_UINT32
        if (type == typeof(ulong)) return 9;       // PATCH_UINT64
        if (type == typeof(bool)) return 10;       // PATCH_BOOL
        if (type == typeof(float)) return 11;      // PATCH_FLOAT
        if (type == typeof(double)) return 12;     // PATCH_DOUBLE
        if (type == typeof(IntPtr)) return 13;     // PATCH_POINTER
        if (type == typeof(object)) return 14;     // PATCH_OBJECT
        if (type == typeof(char)) return 15;       // PATCH_CHAR

        // 其他引用类型
        return type is { IsValueType: false, IsPrimitive: false } ? 14 : // PATCH_OBJECT
            0; // PATCH_NULL
    }

    /// <summary>
    /// 获取类型中的所有内部类型
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="includingParent">是否包含父类型</param>
    /// <param name="outArray">输出的数组指针</param>
    /// <param name="count">数组大小</param>
    /// <returns>执行结果</returns>
    public static bool TypeGetInnerTypes(int typeHandle, bool includingParent, 
        out IntPtr outArray, out int count)
    {
        outArray = IntPtr.Zero;
        count = 0;
        
        if (typeHandle < 0) return false;
        
        var type = Asset.Types[typeHandle];

        try
        {
            // 收集嵌套类型
            var nestedTypes = new List<System.Type>();
            
            // 获取当前类型的嵌套类型
            nestedTypes.AddRange(type.GetNestedTypes(
                BindingFlags.Public | BindingFlags.NonPublic));
            
            // 如果需要，遍历父类型
            if (includingParent)
            {
                var current = type.BaseType;
                while (current != null)
                {
                    nestedTypes.AddRange(current.GetNestedTypes(
                        BindingFlags.Public | BindingFlags.NonPublic));
                    current = current.BaseType;
                }
            }
            
            // 分配数组
            count = nestedTypes.Count;
            if (count == 0) return true; // 空数组也返回成功
            
            var handles = new int[count];
            for (var i = 0; i < count; i++) handles[i] = Asset.Types.Add(nestedTypes[i]);
            
            // 分配非托管内存
            var size = count * Marshal.SizeOf<int>();
            outArray = Marshal.AllocHGlobal(size);
            Marshal.Copy(handles, 0, outArray, count);
            
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 获取类型中的所有方法
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="includingParent">是否包含父类型</param>
    /// <param name="outArray">输出的数组指针</param>
    /// <param name="count">数组大小</param>
    /// <returns>执行结果</returns>
    public static bool TypeGetMethods(int typeHandle, bool includingParent, 
        out IntPtr outArray, out int count)
    {
        outArray = IntPtr.Zero;
        count = 0;
        
        if (typeHandle < 0) return false;
        
        var type = Asset.Types[typeHandle];

        try
        {
            var methods = new List<MemberInfo>();
            
            // 获取绑定标志
            const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | 
                                       BindingFlags.Instance | BindingFlags.Static;

            CollectMethods(type);
            
            // 如果需要，遍历父类型
            if (includingParent)
            {
                var current = type.BaseType;
                while (current != null)
                {
                    CollectMethods(current);
                    current = current.BaseType;
                }
            }
            
            // 分配数组
            count = methods.Count;
            if (count == 0) return true;
            
            var handles = new int[count];
            for (var i = 0; i < count; i++)
            {
                handles[i] = Asset.MethodInfos.Add(methods[i]);
            }
            
            var size = count * Marshal.SizeOf<int>();
            outArray = Marshal.AllocHGlobal(size);
            Marshal.Copy(handles, 0, outArray, count);
            
            return true;

            // 收集方法和构造函数
            void CollectMethods(System.Type t)
            {
                // 收集普通方法
                methods.AddRange(t.GetMethods(flags));
    
                // 收集构造函数
                var constructors = t.GetConstructors(flags);
                foreach (var constructor in constructors)
                {
                    methods.Add(constructor);
                }
            }
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 获取类型中的所有字段
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="includingParent">是否包含父类型</param>
    /// <param name="outArray">输出的数组指针</param>
    /// <param name="count">数组大小</param>
    /// <returns>执行结果</returns>
    public static bool TypeGetFields(int typeHandle, bool includingParent, 
        out IntPtr outArray, out int count)
    {
        outArray = IntPtr.Zero;
        count = 0;
        
        if (typeHandle < 0) return false;
        
        var type = Asset.Types[typeHandle];

        try
        {
            var fields = new List<FieldInfo>();
            
            const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | 
                                       BindingFlags.Instance | BindingFlags.Static;

            CollectFields(type);
            
            if (includingParent)
            {
                var current = type.BaseType;
                while (current != null)
                {
                    CollectFields(current);
                    current = current.BaseType;
                }
            }
            
            count = fields.Count;
            if (count == 0) return true;
            
            var handles = new int[count];
            for (int i = 0; i < count; i++)
            {
                handles[i] = Asset.FieldInfos.Add(fields[i]);
            }
            
            int size = count * Marshal.SizeOf<int>();
            outArray = Marshal.AllocHGlobal(size);
            Marshal.Copy(handles, 0, outArray, count);
            
            return true;

            void CollectFields(System.Type t)
            {
                fields.AddRange(t.GetFields(flags));
            }
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 获取类型中的所有属性
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <param name="includingParent">是否包含父类型</param>
    /// <param name="outArray">输出的数组指针</param>
    /// <param name="count">数组大小</param>
    /// <returns>执行结果</returns>
    public static bool TypeGetProperties(int typeHandle, bool includingParent, 
        out IntPtr outArray, out int count)
    {
        outArray = IntPtr.Zero;
        count = 0;
        
        if (typeHandle < 0) return false;
        
        var type = Asset.Types[typeHandle];

        try
        {
            var properties = new List<PropertyInfo>();
            
            const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | 
                                       BindingFlags.Instance | BindingFlags.Static;

            CollectProperties(type);
            
            if (includingParent)
            {
                var current = type.BaseType;
                while (current != null)
                {
                    CollectProperties(current);
                    current = current.BaseType;
                }
            }
            
            count = properties.Count;
            if (count == 0) return true;
            
            var handles = new int[count];
            for (int i = 0; i < count; i++)
            {
                handles[i] = Asset.PropertyInfos.Add(properties[i]);
            }
            
            int size = count * Marshal.SizeOf<int>();
            outArray = Marshal.AllocHGlobal(size);
            Marshal.Copy(handles, 0, outArray, count);
            
            return true;

            void CollectProperties(System.Type t)
            {
                properties.AddRange(t.GetProperties(flags));
            }
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 卸载类型句柄
    /// </summary>
    /// <param name="typeHandle">类型句柄</param>
    /// <returns>执行结果</returns>
    public static bool TypeFree(int typeHandle)
    {
        if (typeHandle < 0) return false;
        
        Asset.Types.RemoveAt(typeHandle);
        return true;
    }

    public static bool ObjectFree(int objectHandle)
    {
        if (objectHandle < 0) return false;
        
        Asset.Objects.RemoveAt(objectHandle);
        return true;
    }

    public static int ObjectPersist(int objectHandle)
    {
        var o = Asset.Objects[objectHandle];
        return Asset.Objects.Add(o);
    }
}