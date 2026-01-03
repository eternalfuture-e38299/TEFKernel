// /*******************************************************************************
//  * tefkernel - Method.cs
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

using System.Reflection;
using System.Runtime.InteropServices;

namespace tefloader.NetApi;

public static class Method
{
    public static string GetName(int methodHandle) => Asset.MethodInfos[methodHandle].Name;

    public static int GetParamCount(int methodHandle)
    {
        var memberInfo = Asset.MethodInfos[methodHandle];
        if (memberInfo is MethodInfo methodInfo)
            return methodInfo.GetParameters().Length;
        
        return ((ConstructorInfo)memberInfo).GetParameters().Length;
    }

    public static bool IsInstance(int methodHandle)
    {
        
        var memberInfo = Asset.MethodInfos[methodHandle];
        if (memberInfo is MethodInfo methodInfo)
            return !methodInfo.IsStatic;
        
        return !((ConstructorInfo)memberInfo).IsStatic;
    }

    public static int MakeGeneric(int methodHandle, IntPtr genericTypes, int typesSize)
    {
        var genericTypesArray = Utils.CArrayToNetArray<int>(genericTypes, typesSize);
        
        if (methodHandle < 0 || genericTypes == IntPtr.Zero || typesSize <= 0) 
            return -1;
        
        var genericMethodDef = (MethodInfo)Asset.MethodInfos[methodHandle];
        if (!genericMethodDef.IsGenericMethod) 
            return -1;
        
        var typeArgs = new System.Type[typesSize];
        for (var i = 0; i < typesSize; i++)
        {
            var argType = Asset.Types[genericTypesArray[i]];
            typeArgs[i] = argType;
        }
        
        try
        {
            var genericType = genericMethodDef.MakeGenericMethod(typeArgs);
            return Asset.MethodInfos.Add(genericType);
        }
        catch
        {
            return -1;
        }
    }
    
    public static int GetReturnType(int methodHandle)
    {
        if (methodHandle < 0 || methodHandle >= Asset.MethodInfos.Count)
            return -1;
                
        var memberInfo = Asset.MethodInfos[methodHandle];
            
        if (memberInfo is MethodInfo methodInfo)
        {
            var returnType = methodInfo.ReturnType;
            return Asset.Types.Add(returnType);
        }

        if (memberInfo is ConstructorInfo)
        {
            return Asset.Types.Add(typeof(void));
        }

        return -1;
    }
    
    public static int GetParam(int methodHandle, int index)
    {
        if (methodHandle < 0 || methodHandle >= Asset.MethodInfos.Count)
            return -1;
                
        var memberInfo = Asset.MethodInfos[methodHandle];

        var parameters = memberInfo switch
        {
            MethodInfo methodInfo => methodInfo.GetParameters(),
            ConstructorInfo constructorInfo => constructorInfo.GetParameters(),
            _ => null
        };

        if (parameters == null || index < 0 || index >= parameters.Length)
            return -1;
                
        var paramType = parameters[index].ParameterType;
        return Asset.Types.Add(paramType);
    }
    
    public static string GetParamName(int methodHandle, int index)
    {
        if (methodHandle < 0 || methodHandle >= Asset.MethodInfos.Count)
            return string.Empty;
                
        var memberInfo = Asset.MethodInfos[methodHandle];

        var parameters = memberInfo switch
        {
            MethodInfo methodInfo => methodInfo.GetParameters(),
            ConstructorInfo constructorInfo => constructorInfo.GetParameters(),
            _ => null
        };

        if (parameters == null || index < 0 || index >= parameters.Length)
            return string.Empty;
                
        var paramName = parameters[index].Name ?? string.Empty;
        return paramName;
    }
    
    public static bool Invoke(int methodHandle, int instanceHandle, int argCount, 
                              IntPtr returnValue, IntPtr argsPtr, IntPtr typesPtr)
    {
        if (methodHandle < 0 || methodHandle >= Asset.MethodInfos.Count)
                return false;
                
        var memberInfo = Asset.MethodInfos[methodHandle];
        var instance = instanceHandle == -1 ? null : Asset.Objects[instanceHandle];
            
        // 转换参数指针数组
        var argsArray = new IntPtr[argCount];
        if (argsPtr != IntPtr.Zero && argCount > 0)
        {
            Marshal.Copy(argsPtr, argsArray, 0, argCount);
        }
            
        // 转换类型数组
        var typeArray = new int[argCount];
        if (typesPtr != IntPtr.Zero && argCount > 0)
        {
            Marshal.Copy(typesPtr, typeArray, 0, argCount);
        }
            
        // 准备参数
        var parameters = new object?[argCount];
        for (var i = 0; i < argCount; i++)
        {
            if (argsArray[i] == IntPtr.Zero)
            {
                parameters[i] = null;
            }
            else
            {
                // 获取参数类型
                var paramTypeHandle = typeArray[i];
                var paramType = paramTypeHandle >= 0 && paramTypeHandle < Asset.Types.Count 
                    ? Asset.Types[paramTypeHandle] 
                    : typeof(object);
                    
                // 从指针读取值
                parameters[i] = Utils.GetNativeValue(argsArray[i], paramType);
            }
        }
            
        // 调用方法
        var result = memberInfo switch
        {
            MethodInfo methodInfo => methodInfo.Invoke(instance, parameters),
            ConstructorInfo constructorInfo => constructorInfo.Invoke(parameters),
            _ => null
        };

        // 处理返回值
        if (returnValue == IntPtr.Zero) return true;
        var returnType = memberInfo is MethodInfo method 
            ? method.ReturnType 
            : memberInfo.DeclaringType;
                    
        if (returnType != typeof(void) && result != null)
        {
            return Utils.SetNativeValue(returnValue, result);
        }

        return true;
    }

    public static ushort HookMethod(int methodHandle, IntPtr methodSignature, IntPtr prefixHook, IntPtr postfixHook) =>
        HookManager.HookMethod((MethodBase)Asset.MethodInfos[methodHandle], methodSignature, prefixHook, postfixHook);

    public static bool UnHookMethod(ushort nodeIndex) => HookManager.UnhookMethodByNode(nodeIndex);

    public static bool HasSingleHookNode(int methodHandle) => HookManager.HasSingleHookNode((MethodBase)Asset.MethodInfos[methodHandle]);

    public static bool IsMethodHooked(int methodHandle) => HookManager.IsMethodHooked((MethodBase)Asset.MethodInfos[methodHandle]);

    public static IntPtr GetHookedMethodSig(int methodHandle) => HookManager.GetMethodSig((MethodBase)Asset.MethodInfos[methodHandle]);

    public static int GetMethodByNode(ushort nodeIndex) => HookManager.GetMethodByNode(nodeIndex);
    
    public static void Free(int methodHandle)
    {
        Asset.MethodInfos.RemoveAt(methodHandle);
    }
}