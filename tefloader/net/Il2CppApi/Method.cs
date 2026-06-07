// /*******************************************************************************
//  * tefloader - Method.cs
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

public static class Method
{
    // 模拟 il2cpp_method_get_name
    public static string? il2cpp_method_get_name(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return null;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        // 静态构造函数
        if (method.IsStatic && method.IsConstructor)
            return ".cctor";

        // 实例构造函数
        if (method.IsConstructor)
            return ".ctor";

        return method.Name;
    }

    // 模拟 il2cpp_method_get_param_count
    public static uint il2cpp_method_get_param_count(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return 0;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        return (uint)method.GetParameters().Length;
    }

    // 模拟 il2cpp_method_get_param_name
    public static string? il2cpp_method_get_param_name(IntPtr methodPtr, uint index)
    {
        if (methodPtr == IntPtr.Zero)
            return null;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        var parameters = method.GetParameters();
        return index >= parameters.Length ? null : parameters[index].Name;
    }

    // 模拟 il2cpp_method_get_param
    public static IntPtr il2cpp_method_get_param(IntPtr methodPtr, uint index)
    {
        if (methodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        var parameters = method.GetParameters();
        if (index >= parameters.Length)
            return IntPtr.Zero;

        var paramType = parameters[index].ParameterType;
        var paramHandle = GCHandle.Alloc(paramType);
        return GCHandle.ToIntPtr(paramHandle);
    }

    // 模拟 il2cpp_method_is_instance
    public static bool il2cpp_method_is_instance(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return false;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        // 构造函数是实例方法
        if (method.IsConstructor && !method.IsStatic)
            return true;

        return !method.IsStatic;
    }

    // 模拟 il2cpp_method_is_generic
    public static bool il2cpp_method_is_generic(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return false;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        // 构造函数不能是泛型方法
        if (method.IsConstructor)
            return false;

        var methodInfo = method as MethodInfo;
        if (methodInfo == null)
            return false;

        return methodInfo.IsGenericMethod;
    }

    // 模拟 il2cpp_method_get_return_type
    public static IntPtr il2cpp_method_get_return_type(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        Type returnType;

        // 构造函数返回 void
        if (method.IsConstructor)
        {
            returnType = typeof(void);
        }
        else
        {
            var methodInfo = method as MethodInfo;
            if (methodInfo == null)
                return IntPtr.Zero;
            returnType = methodInfo.ReturnType;
        }

        var returnHandle = GCHandle.Alloc(returnType);
        return GCHandle.ToIntPtr(returnHandle);
    }

    // 模拟 il2cpp_method_get_declaring_type
    public static IntPtr il2cpp_method_get_declaring_type(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        var declaringType = method.DeclaringType;
        if (declaringType == null)
            return IntPtr.Zero;

        var declaringHandle = GCHandle.Alloc(declaringType);
        return GCHandle.ToIntPtr(declaringHandle);
    }

    // 模拟 il2cpp_method_get_class
    public static IntPtr il2cpp_method_get_class(IntPtr methodPtr)
    {
        return il2cpp_method_get_declaring_type(methodPtr);
    }

    // 模拟 il2cpp_method_get_object
    /*public static IntPtr il2cpp_method_get_object(IntPtr methodPtr, IntPtr refclassPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        // 创建反射方法对象
        var methodHandleNew = GCHandle.Alloc(method);
        return GCHandle.ToIntPtr(methodHandleNew);
    }*/

    // 模拟 il2cpp_method_get_from_reflection
    /*public static IntPtr il2cpp_method_get_from_reflection(IntPtr reflectionMethodPtr)
    {
        if (reflectionMethodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        var reflectionHandle = GCHandle.FromIntPtr(reflectionMethodPtr);
        var reflectionMethod = reflectionHandle.Target;

        if (reflectionMethod is MethodBase method)
        {
            var methodHandle = GCHandle.Alloc(method);
            return GCHandle.ToIntPtr(methodHandle);
        }

        return IntPtr.Zero;
    }*/

    // 模拟 il2cpp_method_get_token
    public static uint il2cpp_method_get_token(IntPtr methodPtr)
    {
        if (methodPtr == IntPtr.Zero)
            return 0;

        var methodHandle = GCHandle.FromIntPtr(methodPtr);
        var method = (MethodBase)methodHandle.Target;

        return (uint)method.MetadataToken;
    }

    // 模拟 il2cpp_runtime_invoke
    public static unsafe bool il2cpp_method_invoke(IntPtr methodPtr, IntPtr objPtr, IntPtr* paramsPtr,
        IntPtr returnValuePtr)
    {
        if (methodPtr == IntPtr.Zero)
            return false;

        try
        {
            var method = (MethodBase)Utils.PtrToObject(methodPtr)!;

            // 获取目标对象
            object? target = null;
            if (objPtr != IntPtr.Zero) target = Utils.PtrToObject(objPtr);

            // 转换参数
            object?[]? parameters = null;
            if (paramsPtr != null)
            {
                var paramInfos = method.GetParameters();
                var paramCount = paramInfos.Length;
                parameters = new object[paramCount];

                for (var i = 0; i < paramCount; i++)
                {
                    var paramValuePtr = paramsPtr[i];
                    if (paramValuePtr == IntPtr.Zero)
                    {
                        parameters[i] = null;
                        continue;
                    }

                    var paramType = paramInfos[i].ParameterType;

                    if (Utils.IsValueType(paramType))
                        // 值类型：直接读取值
                        parameters[i] = Utils.GetNativeValue(paramValuePtr, paramType);
                    else
                        // 引用类型：从 GCHandle 解引用
                        parameters[i] = Utils.PtrToObject(paramValuePtr);
                }
            }

            // 调用方法
            object? result = null;

            if (method.IsConstructor)
            {
                var constructor = method as ConstructorInfo;
                if (constructor != null) result = constructor.Invoke(parameters);
            }
            else
            {
                var methodInfo = method as MethodInfo;
                if (methodInfo != null) result = methodInfo.Invoke(target, parameters);
            }

            // 处理返回值
            if (returnValuePtr == IntPtr.Zero)
                return true;
            
            Type? returnType = null;
            if (method is MethodInfo mi)
                returnType = mi.ReturnType;
            else if (method.IsConstructor)
                returnType = method.DeclaringType;

            if (returnType != null && Utils.IsValueType(returnType))
            {
                // 值类型：直接复制值到 returnValuePtr
                if (returnType != typeof(void)) Utils.SetNativeValue(returnValuePtr, result);
            }
            else if (returnType != null)
            {
                // 引用类型：分配 GCHandle 并复制指针
                if (result == null)
                {
                    *(IntPtr*)returnValuePtr = IntPtr.Zero;
                }
                else
                {
                    var resultPtr = Utils.ObjectToPtr(result);
                    *(IntPtr*)returnValuePtr = resultPtr;
                }
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"il2cpp_runtime_invoke error: {ex.Message}");
            return false;
        }
    }

    // 模拟 il2cpp_method_make_generic - 支持泛型方法和泛型构造函数
    public static unsafe IntPtr il2cpp_method_make_generic(IntPtr methodPtr, IntPtr* typesPtr, int typesCount)
    {
        if (methodPtr == IntPtr.Zero)
            return IntPtr.Zero;

        if (typesCount <= 0 || typesPtr == null)
        {
            Logger.Error("Invalid generic arguments");
            return IntPtr.Zero;
        }

        try
        {
            var methodHandle = GCHandle.FromIntPtr(methodPtr);
            var method = (MethodBase)methodHandle.Target;

            // 获取泛型参数类型
            var genericArgs = new Type[typesCount];
            for (var i = 0; i < typesCount; i++)
            {
                var typeFromPtr = GCHandle.FromIntPtr(typesPtr[i]).Target;
                if (typeFromPtr == null)
                {
                    Logger.Error($"Failed to get generic argument at index {i}");
                    return IntPtr.Zero;
                }

                genericArgs[i] = (Type)typeFromPtr;
            }

            MethodBase? resultMethod;

            // 根据方法类型处理
            switch (method)
            {
                case MethodInfo { IsGenericMethodDefinition: true } mi:
                    // 泛型方法定义 -> 创建泛型方法实例
                    resultMethod = mi.MakeGenericMethod(genericArgs);
                    break;

                case ConstructorInfo { DeclaringType.IsGenericTypeDefinition: true } ci:
                    // 泛型类中的构造函数 -> 先构造泛型类型，再获取构造函数
                    var genericType = ci.DeclaringType.MakeGenericType(genericArgs);
                    // 获取构造函数的参数类型数组
                    var paramTypes = ci.GetParameters().Select(p => p.ParameterType).ToArray();
                    resultMethod = genericType.GetConstructor(paramTypes);
                    break;

                case MethodInfo { DeclaringType.IsGenericTypeDefinition: true } mi:
                    // 泛型类中的方法
                    var declaringType = mi.DeclaringType.MakeGenericType(genericArgs);
                    resultMethod = declaringType.GetMethod(mi.Name,
                        mi.GetParameters().Select(p => p.ParameterType).ToArray());
                    break;

                default:
                    Logger.Error($"Method is not a generic method definition: {method.Name}");
                    return IntPtr.Zero;
            }

            if (resultMethod == null)
            {
                Logger.Error("Failed to create generic method");
                return IntPtr.Zero;
            }

            var resultHandle = GCHandle.Alloc(resultMethod);
            return GCHandle.ToIntPtr(resultHandle);
        }
        catch (Exception ex)
        {
            Logger.Error($"il2cpp_method_make_generic error: {ex.Message}");
            return IntPtr.Zero;
        }
    }
}