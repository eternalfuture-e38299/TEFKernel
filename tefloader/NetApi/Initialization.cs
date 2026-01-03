/*******************************************************************************
 * tefkernel - Initialization.cs
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

namespace tefloader.NetApi;

public static class Initialization
{
    private static readonly List<Delegate> KeepAliveDelegates = [];
    private static readonly List<IntPtr> KeepAlivePointers = [];


    private static void RegisterApiMethod<T>(Delegate method, string name) where T : Delegate
    {
        var delegateInstance = Delegate.CreateDelegate(typeof(T), method.Target, method.Method) as T;
        if (delegateInstance == null)
        {
            Console.WriteLine($"Failed to create delegate for {name}");
            return;
        }

        // 将委托转换为函数指针
        var functionPointer = Marshal.GetFunctionPointerForDelegate(delegateInstance);

        // 设置到CoreLib

        Program.CoreLib.SetVariable(name, functionPointer);
        // 保持委托和指针引用，防止GC回收
        KeepAliveDelegates.Add(delegateInstance);
        KeepAlivePointers.Add(functionPointer);
        Logger.Info($"Successfully registered {name}");
    }

    // 清理方法（可选）
    public static void Cleanup()
    {
        KeepAliveDelegates.Clear();
        KeepAlivePointers.Clear();
    }

    public static class TypeApi
    {
        public static void Init()
        {
            // 注册所有Type API函数
            RegisterApiMethod<NetGetTypeDelegate>(Type.GetType, "net_get_type");
            RegisterApiMethod<NetNewInstanceDelegate>(Type.NewInstance, "net_new_instance");
            RegisterApiMethod<NetTypeMakeGenericDelegate>(Type.TypeMakeGeneric, "net_type_make_generic");
            RegisterApiMethod<NetTypeGetNameDelegate>(Type.TypeGetName, "net_type_get_name");
            RegisterApiMethod<NetTypeGetNamespaceDelegate>(Type.TypeGetNamespace, "net_type_get_namespace");
            RegisterApiMethod<NetTypeGetParentDelegate>(Type.TypeGetParent, "net_type_get_parent");
            RegisterApiMethod<NetTypeGetFieldDelegate>(Type.TypeGetField, "net_type_get_field");
            RegisterApiMethod<NetTypeGetPropertyDelegate>(Type.TypeGetProperty, "net_type_get_property");
            RegisterApiMethod<NetTypeGetMethodFromArgsCountDelegate>(
                Type.TypeGetMethodFromArgsCount, "net_type_get_method_from_args_count");
            RegisterApiMethod<NetTypeToPatchlibTypeDelegate>(Type.TypeToPatchlibType, "net_type_to_patchlib_type");
            RegisterApiMethod<NetTypeGetInnerTypesDelegate>(Type.TypeGetInnerTypes, "net_type_get_inner_types");
            RegisterApiMethod<NetTypeGetMethodsDelegate>(Type.TypeGetMethods, "net_type_get_methods");
            RegisterApiMethod<NetTypeGetFieldsDelegate>(Type.TypeGetFields, "net_type_get_fields");
            RegisterApiMethod<NetTypeGetPropertiesDelegate>(Type.TypeGetProperties, "net_type_get_properties");
            RegisterApiMethod<NetTypeFreeDelegate>(Type.TypeFree, "net_type_free");
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetGetTypeDelegate(string ns, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetNewInstanceDelegate(int type);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetTypeFreeDelegate(int type);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeGetFieldDelegate(int type, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetTypeGetFieldsDelegate(int type, bool includingParent, out IntPtr outArray,
            out int count);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetTypeGetInnerTypesDelegate(int type, bool includingParent, out IntPtr outArray,
            out int count);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeGetMethodFromArgsCountDelegate(int type, string name, int argsCount);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetTypeGetMethodsDelegate(int type, bool includingParent, out IntPtr outArray,
            out int count);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private delegate string NetTypeGetNameDelegate(int type);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr NetTypeGetNamespaceDelegate(int type);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeGetParentDelegate(int type);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetTypeGetPropertiesDelegate(int type, bool includingParent, out IntPtr outArray,
            out int count);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeGetPropertyDelegate(int type, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeMakeGenericDelegate(int type, IntPtr genericTypes, int typesSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetTypeToPatchlibTypeDelegate(int type);
    }

    public static class FieldApi
    {
        public static void Init()
        {
            // 注册所有Field API函数
            RegisterApiMethod<NetFieldGetNameDelegate>(Field.GetName, "net_field_get_name");
            RegisterApiMethod<NetFieldIsStaticDelegate>(Field.IsStatic, "net_field_is_static");
            RegisterApiMethod<NetFieldIsConstDelegate>(Field.IsConst, "net_field_is_const");
            RegisterApiMethod<NetFieldIsThreadStaticDelegate>(Field.IsThreadStatic, "net_field_is_thread_static");
            RegisterApiMethod<NetFieldGetValueDelegate>(Field.GetValue, "net_field_get_value");
            RegisterApiMethod<NetFieldSetValueDelegate>(Field.SetValue, "net_field_set_value");
            RegisterApiMethod<NetFieldFreeDelegate>(Field.Free, "net_field_free");
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private delegate string NetFieldGetNameDelegate(int fieldHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetFieldIsStaticDelegate(int fieldHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetFieldIsConstDelegate(int fieldHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetFieldIsThreadStaticDelegate(int fieldHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetFieldGetValueDelegate(int fieldHandle, int instanceHandle, IntPtr valueOut);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetFieldSetValueDelegate(int fieldHandle, int instanceHandle, IntPtr valuePtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NetFieldFreeDelegate(int fieldHandle);
    }

    public static class PropertyApi
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private delegate string NetPropertyGetNameDelegate(int propertyHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetPropertyGetGetMethodDelegate(int propertyHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetPropertyGetSetMethodDelegate(int propertyHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NetPropertyFreeDelegate(int propertyHandle);

        public static void Init()
        {
            // 注册所有Property API函数
            RegisterApiMethod<NetPropertyGetNameDelegate>(Property.GetName, "net_property_get_name");
            RegisterApiMethod<NetPropertyGetGetMethodDelegate>(Property.GetGetMethod,
                "net_property_get_get_method");
            RegisterApiMethod<NetPropertyGetSetMethodDelegate>(Property.GetSetMethod,
                "net_property_get_set_method");
            RegisterApiMethod<NetPropertyFreeDelegate>(Property.Free, "net_property_free");
        }
    }

    public static class MethodInit
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        private delegate string NetMethodGetNameDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetMethodGetParamCountDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetMethodIsInstanceDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetMethodMakeGenericMethodDelegate(int methodHandle, IntPtr genericTypes,
            int typesSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetMethodGetReturnTypeDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetMethodGetParamDelegate(int methodHandle, int index);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string NetMethodGetParamNameDelegate(int methodHandle, int index);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private delegate bool NetMethodInvokeDelegate(int methodHandle, int instanceHandle, int argCount,
            IntPtr returnValue, IntPtr argsPtr, IntPtr typesPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NetMethodFreeDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate ushort NetHookMethodDelegate(int methodHandle, IntPtr methodSignature, 
            IntPtr prefixHook, IntPtr postfixHook);
    
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool NetUnhookMethodDelegate(ushort nodeIndex);
    
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool NetHasSingleHookNodeDelegate(int methodHandle);
    
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool NetIsMethodHookedDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr NetGetHookedMethodSigDelegate(int methodHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int NetGetMethodByNodeDelegate(ushort nodeIndex);
        
        
        public static void Init()
        {
            // 注册所有Method API函数
            RegisterApiMethod<NetMethodGetNameDelegate>(Method.GetName, "net_method_get_name");
            RegisterApiMethod<NetMethodGetParamCountDelegate>(Method.GetParamCount, "net_method_get_param_count");
            RegisterApiMethod<NetMethodIsInstanceDelegate>(Method.IsInstance, "net_method_is_instance");
            RegisterApiMethod<NetMethodMakeGenericMethodDelegate>(Method.MakeGeneric,
                "net_method_make_generic_method");
            RegisterApiMethod<NetMethodGetReturnTypeDelegate>(Method.GetReturnType, "net_method_get_return_type");
            RegisterApiMethod<NetMethodGetParamDelegate>(Method.GetParam, "net_method_get_param");
            RegisterApiMethod<NetMethodGetParamNameDelegate>(Method.GetParamName, "net_method_get_param_name");
            RegisterApiMethod<NetMethodInvokeDelegate>(Method.Invoke, "net_method_invoke");
            RegisterApiMethod<NetMethodFreeDelegate>(Method.Free, "net_method_free");
            
            RegisterApiMethod<NetHookMethodDelegate>(Method.HookMethod, "net_hook_method");
            RegisterApiMethod<NetUnhookMethodDelegate>(Method.UnHookMethod, "net_unhook_method");
            RegisterApiMethod<NetHasSingleHookNodeDelegate>(Method.HasSingleHookNode, "net_has_single_hook_node");
            RegisterApiMethod<NetIsMethodHookedDelegate>(Method.IsMethodHooked, "net_is_method_hooked");
            RegisterApiMethod<NetGetHookedMethodSigDelegate>(Method.GetHookedMethodSig, "net_get_hooked_method_sig");
            RegisterApiMethod<NetGetMethodByNodeDelegate>(Method.GetMethodByNode, "net_get_method_by_hook_node");
        }
    }
}