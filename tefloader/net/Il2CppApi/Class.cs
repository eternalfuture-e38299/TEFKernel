// /*******************************************************************************
//  * tefloader - Class.cs
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

public static class Class
{
    public enum Il2CppTypeEnum
    {
        IL2CPP_TYPE_END = 0x00,
        IL2CPP_TYPE_VOID = 0x01,
        IL2CPP_TYPE_BOOLEAN = 0x02,
        IL2CPP_TYPE_CHAR = 0x03,
        IL2CPP_TYPE_I1 = 0x04,
        IL2CPP_TYPE_U1 = 0x05,
        IL2CPP_TYPE_I2 = 0x06,
        IL2CPP_TYPE_U2 = 0x07,
        IL2CPP_TYPE_I4 = 0x08,
        IL2CPP_TYPE_U4 = 0x09,
        IL2CPP_TYPE_I8 = 0x0a,
        IL2CPP_TYPE_U8 = 0x0b,
        IL2CPP_TYPE_R4 = 0x0c,
        IL2CPP_TYPE_R8 = 0x0d,
        IL2CPP_TYPE_STRING = 0x0e,
        IL2CPP_TYPE_PTR = 0x0f,
        IL2CPP_TYPE_BYREF = 0x10,
        IL2CPP_TYPE_VALUETYPE = 0x11,
        IL2CPP_TYPE_CLASS = 0x12,
        IL2CPP_TYPE_VAR = 0x13,
        IL2CPP_TYPE_ARRAY = 0x14,
        IL2CPP_TYPE_GENERICINST = 0x15,
        IL2CPP_TYPE_TYPEDBYREF = 0x16,
        IL2CPP_TYPE_I = 0x18,
        IL2CPP_TYPE_U = 0x19,
        IL2CPP_TYPE_FNPTR = 0x1b,
        IL2CPP_TYPE_OBJECT = 0x1c,
        IL2CPP_TYPE_SZARRAY = 0x1d,
        IL2CPP_TYPE_MVAR = 0x1e,
        IL2CPP_TYPE_CMOD_REQD = 0x1f,
        IL2CPP_TYPE_CMOD_OPT = 0x20,
        IL2CPP_TYPE_INTERNAL = 0x21,
        IL2CPP_TYPE_MODIFIER = 0x40,
        IL2CPP_TYPE_SENTINEL = 0x41,
        IL2CPP_TYPE_PINNED = 0x45,
        IL2CPP_TYPE_ENUM = 0x55
    }
    
    // 模拟 il2cpp_class_from_name
    public static IntPtr il2cpp_class_from_name(IntPtr imagePtr, string namespaze, string name)
    {
        var imageHandle = GCHandle.FromIntPtr(imagePtr);
        var image = (Assembly)imageHandle.Target;
        
        // 从image中获取类型
        var type = image.GetType($"{namespaze}.{name}");
        if (type == null) return IntPtr.Zero;
        
        var classHandle = GCHandle.Alloc(type);
        return GCHandle.ToIntPtr(classHandle);
    }
    
    // 模拟 il2cpp_class_get_nested_types (C风格数组)
    public static unsafe IntPtr* il2cpp_class_get_nested_types(IntPtr classPtr, out int size)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var nestedTypes = type.GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic);
        var nestedPtrs = new IntPtr[nestedTypes.Length];
        
        for (var i = 0; i < nestedTypes.Length; i++)
        {
            var nestedHandle = GCHandle.Alloc(nestedTypes[i]);
            nestedPtrs[i] = GCHandle.ToIntPtr(nestedHandle);
        }
        
        size = nestedPtrs.Length;
        
        var ptr = (IntPtr*)Marshal.AllocHGlobal(IntPtr.Size * size);
        for (var i = 0; i < size; i++) ptr[i] = nestedPtrs[i];
        
        return ptr;
    }
    
    // 模拟 il2cpp_class_get_name
    public static string il2cpp_class_get_name(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.Name;
    }
    
    // 模拟 il2cpp_class_get_parent
    public static IntPtr il2cpp_class_get_parent(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var baseType = type.BaseType;
        if (baseType == null) return IntPtr.Zero;
        
        var baseHandle = GCHandle.Alloc(baseType);
        return GCHandle.ToIntPtr(baseHandle);
    }
    
    // 模拟 il2cpp_class_get_methods (C风格数组) - 包含构造函数和静态构造函数
    public static unsafe IntPtr* il2cpp_class_get_methods(IntPtr classPtr, out int size)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
    
        // 获取所有普通方法
        var methods = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | 
                                      BindingFlags.Instance | BindingFlags.Static |
                                      BindingFlags.DeclaredOnly);
    
        // 获取所有实例构造函数
        var instanceConstructors = type.GetConstructors(BindingFlags.Public | BindingFlags.NonPublic |
                                                        BindingFlags.Instance);
    
        // 获取静态构造函数（如果有）
        var staticConstructor = type.TypeInitializer;
    
        // 计算总数：普通方法 + 实例构造函数 + 静态构造函数（如果存在）
        var totalCount = methods.Length + instanceConstructors.Length;
        var hasStaticCtor = staticConstructor != null;
        if (hasStaticCtor)
            totalCount++;
    
        // 合并所有方法
        var allMethods = new MethodBase?[totalCount];
        var index = 0;
    
        // 添加普通方法
        methods.CopyTo(allMethods, index);
        index += methods.Length;
    
        // 添加实例构造函数
        instanceConstructors.CopyTo(allMethods, index);
        index += instanceConstructors.Length;
    
        // 添加静态构造函数（如果存在）
        if (hasStaticCtor) allMethods[index] = staticConstructor;
    
        var methodPtrs = new IntPtr[allMethods.Length];
    
        for (var i = 0; i < allMethods.Length; i++)
        {
            var methodHandle = GCHandle.Alloc(allMethods[i]);
            methodPtrs[i] = GCHandle.ToIntPtr(methodHandle);
        }
    
        size = methodPtrs.Length;
    
        var ptr = (IntPtr*)Marshal.AllocHGlobal(IntPtr.Size * size);
        for (var i = 0; i < size; i++) ptr[i] = methodPtrs[i];
    
        return ptr;
    }
    
    // 模拟 il2cpp_class_get_fields (C风格数组)
    public static unsafe IntPtr* il2cpp_class_get_fields(IntPtr classPtr, out int size)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var fields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | 
                                     BindingFlags.Instance | BindingFlags.Static |
                                     BindingFlags.DeclaredOnly);
        
        var fieldPtrs = new IntPtr[fields.Length];
        
        for (int i = 0; i < fields.Length; i++)
        {
            var fieldHandle = GCHandle.Alloc(fields[i]);
            fieldPtrs[i] = GCHandle.ToIntPtr(fieldHandle);
        }
        
        size = fieldPtrs.Length;
        
        var ptr = (IntPtr*)Marshal.AllocHGlobal(IntPtr.Size * size);
        for (int i = 0; i < size; i++)
        {
            ptr[i] = fieldPtrs[i];
        }
        
        return ptr;
    }
    
    // 模拟 il2cpp_class_get_properties (C风格数组)
    public static unsafe IntPtr* il2cpp_class_get_properties(IntPtr classPtr, out int size)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var properties = type.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | 
                                             BindingFlags.Instance | BindingFlags.Static |
                                             BindingFlags.DeclaredOnly);
        
        var propPtrs = new IntPtr[properties.Length];
        
        for (var i = 0; i < properties.Length; i++)
        {
            var propHandle = GCHandle.Alloc(properties[i]);
            propPtrs[i] = GCHandle.ToIntPtr(propHandle);
        }
        
        size = propPtrs.Length;
        
        var ptr = (IntPtr*)Marshal.AllocHGlobal(IntPtr.Size * size);
        for (var i = 0; i < size; i++) ptr[i] = propPtrs[i];
        
        return ptr;
    }
    
    // 模拟 il2cpp_class_get_static_field_data
    // 此方法无法实现
    /*public static IntPtr il2cpp_class_get_static_field_data(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        // 获取所有静态字段
        var staticFields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | 
                                           BindingFlags.Static);
        
        // 创建一个对象来存储静态字段的值
        var staticData = new Dictionary<string, object>();
        foreach (var field in staticFields)
        {
            staticData[field.Name] = field.GetValue(null);
        }
        
        var dataHandle = GCHandle.Alloc(staticData);
        return GCHandle.ToIntPtr(dataHandle);
    }
    */
    
    
    // 模拟 il2cpp_class_get_namespace
    public static string? il2cpp_class_get_namespace(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.Namespace;
    }
    
    // 模拟 il2cpp_class_get_type
    /*public static IntPtr il2cpp_class_get_type(IntPtr classPtr)
    {
        // 返回类型自身的指针
        return classPtr;
    }*/
    
    // 模拟 il2cpp_class_is_abstract
    public static bool il2cpp_class_is_abstract(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.IsAbstract;
    }
    
    // 模拟 il2cpp_class_is_interface
    public static bool il2cpp_class_is_interface(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.IsInterface;
    }
    
    // 模拟 il2cpp_class_is_enum
    public static bool il2cpp_class_is_enum(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.IsEnum;
    }
    
    // 模拟 il2cpp_class_is_generic
    public static bool il2cpp_class_is_generic(IntPtr classPtr)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        return type.IsGenericType;
    }
    
    // 模拟 il2cpp_class_from_system_type
    /*public static IntPtr il2cpp_class_from_system_type(IntPtr systemTypePtr)
    {
        var systemTypeHandle = GCHandle.FromIntPtr(systemTypePtr);
        var type = (Type)systemTypeHandle.Target;

        var classHandle = GCHandle.Alloc(type);
        return GCHandle.ToIntPtr(classHandle);
    }*/
    
    // 模拟 il2cpp_class_get_field_from_name
    public static IntPtr il2cpp_class_get_field_from_name(IntPtr classPtr, string name)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var field = type.GetField(name, BindingFlags.Public | BindingFlags.NonPublic | 
                                      BindingFlags.Instance | BindingFlags.Static);
        
        if (field == null) return IntPtr.Zero;
        
        var fieldHandle = GCHandle.Alloc(field);
        return GCHandle.ToIntPtr(fieldHandle);
    }
    
    // 模拟 il2cpp_class_get_property_from_name
    public static IntPtr il2cpp_class_get_property_from_name(IntPtr classPtr, string name)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        var property = type.GetProperty(name, BindingFlags.Public | BindingFlags.NonPublic | 
                                           BindingFlags.Instance | BindingFlags.Static);
        
        if (property == null) return IntPtr.Zero;
        
        var propHandle = GCHandle.Alloc(property);
        return GCHandle.ToIntPtr(propHandle);
    }
    
    // 模拟 il2cpp_class_get_method_from_name - 支持构造函数
    public static IntPtr il2cpp_class_get_method_from_name(IntPtr classPtr, string name, int argsCount)
    {
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;

        switch (name)
        {
            // 处理实例构造函数
            case ".ctor":
            {
                var constructors = type.GetConstructors(BindingFlags.Public | BindingFlags.NonPublic | 
                                                        BindingFlags.Instance);
                var constructor = constructors.FirstOrDefault(c => c.GetParameters().Length == argsCount);
                if (constructor == null) return IntPtr.Zero;
                var ctorHandle = GCHandle.Alloc(constructor);
                return GCHandle.ToIntPtr(ctorHandle);
            }
            // 处理静态构造函数
            case ".cctor":
            {
                // 静态构造函数需要通过 TypeInitializer 获取
                var constructor = type.TypeInitializer;
                if (constructor == null) return IntPtr.Zero;
                // 静态构造函数没有参数，忽略 argsCount
                var cctorHandle = GCHandle.Alloc(constructor);
                return GCHandle.ToIntPtr(cctorHandle);
            }
        }

        // 普通方法
        var methods = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | 
                                      BindingFlags.Instance | BindingFlags.Static);
    
        var method = methods.FirstOrDefault(m => m.Name == name && m.GetParameters().Length == argsCount);
    
        if (method == null) return IntPtr.Zero;
    
        var methodHandle = GCHandle.Alloc(method);
        return GCHandle.ToIntPtr(methodHandle);
    }
    
    // 模拟 il2cpp_class_from_il2cpp_type
    // c层抽象宏，不做任何事情
    /*public static IntPtr il2cpp_class_from_il2cpp_type(IntPtr typePtr)
    {
        // typePtr 指向 Il2CppType 结构，这里直接返回 classPtr
        return typePtr;
    }
    */
    
    public static IntPtr il2cpp_object_new(IntPtr classPtr)
    {
        if (classPtr == IntPtr.Zero)
            return IntPtr.Zero;
            
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;
        
        try
        {
            // 检查类型是否可以实例化
            if (type.IsAbstract || type.IsInterface)
            {
                throw new InvalidOperationException($"Cannot create instance of abstract class or interface: {type.FullName}");
            }
            
            if (type.ContainsGenericParameters)
            {
                throw new InvalidOperationException($"Cannot create instance of generic type with open parameters: {type.FullName}");
            }
            
            // 创建对象实例（不调用构造函数）

            var instance =
                // 对于值类型，直接创建默认实例
                type.IsValueType ? Activator.CreateInstance(type) :
                // 对于引用类型，使用 FormatterServices 创建未初始化的对象（不调用构造函数）
                // 使用 FormatterServices 创建对象而不调用构造函数
                System.Runtime.Serialization.FormatterServices.GetUninitializedObject(type);
            
            if (instance == null)
                return IntPtr.Zero;
            
            var objHandle = GCHandle.Alloc(instance);
            var objPtr = GCHandle.ToIntPtr(objHandle);
            
            return objPtr;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error creating instance of type {type.FullName}: {ex.Message}");
            return IntPtr.Zero;
        }
    }

    public static unsafe IntPtr il2cpp_class_make_generic(IntPtr classPtr, IntPtr *typesPtr, int typesCount)
    {
        if (classPtr == IntPtr.Zero)
            return IntPtr.Zero;
            
        var classHandle = GCHandle.FromIntPtr(classPtr);
        var type = (Type)classHandle.Target;

        var types = new Type[typesCount];
        if (types == null) throw new ArgumentNullException(nameof(types));
        for (var i = 0; i < typesCount; i++)
        {
            var typeFromPtr = GCHandle.FromIntPtr(typesPtr[i]).Target;
            if (typeFromPtr == null) return IntPtr.Zero;
            types[i] = (Type)typeFromPtr;
        }

        var genericType = type.MakeGenericType(types);
        
        var genericTypeHandle = GCHandle.Alloc(genericType);
        var genericTypePtr = GCHandle.ToIntPtr(genericTypeHandle);

        return genericTypePtr;
    }

    public static bool il2cpp_class_is_same(IntPtr c1, IntPtr c2)
    {
        if (c1 == c2)
            return true;
    
        if (c1 == IntPtr.Zero || c2 == IntPtr.Zero)
            return false;
    
        try
        {
            var t1 = (Type)GCHandle.FromIntPtr(c1).Target;
            var t2 = (Type)GCHandle.FromIntPtr(c2).Target;
        
            return t1 == t2 || t1.FullName == t2.FullName;
        }
        catch
        {
            return false;
        }
    }
    
     // 模拟 il2cpp_type_get_type
    public static int il2cpp_type_get_type(IntPtr typePtr)
    {
        if (typePtr == IntPtr.Zero)
            return (int)Il2CppTypeEnum.IL2CPP_TYPE_END;
        
        try
        {
            // 如果 typePtr 指向的是 Type 对象
            var typeHandle = GCHandle.FromIntPtr(typePtr);
            var type = (System.Type)typeHandle.Target;
            
            // 根据 .NET Type 映射到 IL2CPP 类型枚举
            if (type == typeof(void))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_VOID;
            
            if (type == typeof(bool))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_BOOLEAN;
            
            if (type == typeof(char))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_CHAR;
            
            if (type == typeof(sbyte))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_I1;
            
            if (type == typeof(byte))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_U1;
            
            if (type == typeof(short))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_I2;
            
            if (type == typeof(ushort))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_U2;
            
            if (type == typeof(int))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_I4;
            
            if (type == typeof(uint))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_U4;
            
            if (type == typeof(long))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_I8;
            
            if (type == typeof(ulong))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_U8;
            
            if (type == typeof(float))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_R4;
            
            if (type == typeof(double))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_R8;
            
            if (type == typeof(string))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_STRING;
            
            if (type.IsPointer)
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_PTR;
            
            if (type.IsByRef)
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_BYREF;
            
            if (type is { IsValueType: true, IsEnum: false })
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_VALUETYPE;
            
            if (type.IsClass && type is { IsArray: false, IsGenericType: false })
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_CLASS;
            
            if (type.IsArray)
            {
                if (type.GetArrayRank() == 1)
                    return (int)Il2CppTypeEnum.IL2CPP_TYPE_SZARRAY;
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_ARRAY;
            }
            
            if (type.IsGenericType)
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_GENERICINST;
            
            if (type.IsEnum)
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_ENUM;
            
            if (type == typeof(object))
                return (int)Il2CppTypeEnum.IL2CPP_TYPE_OBJECT;
            
            // 默认返回 CLASS 类型
            return (int)Il2CppTypeEnum.IL2CPP_TYPE_CLASS;
        }
        catch
        {
            return (int)Il2CppTypeEnum.IL2CPP_TYPE_END;
        }
    }
}