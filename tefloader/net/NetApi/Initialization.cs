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
    private static readonly List<IntPtr> KeepAlivePointers = [];
    private static readonly List<Delegate> KeepAliveDelegates = [];

    private static void RegisterApiMethod<T>(T method, string name) where T : Delegate
    {
        var functionPointer = Marshal.GetFunctionPointerForDelegate(method);
        Program.TefKernelLib.SetVariable(name, functionPointer);
        KeepAlivePointers.Add(functionPointer);
        KeepAliveDelegates.Add(method);
        Logger.Info($"Successfully registered {name}");
    }

    public static void Cleanup()
    {
        KeepAlivePointers.Clear();
        KeepAliveDelegates.Clear();
    }

    public static class TypeApi
    {
        // Type API 委托
        private delegate int GetTypeDelegate(string ns, string name);
        private delegate int NewInstanceDelegate(int typeHandle);

        private delegate int TypeMakeGenericDelegate(int typeHandle, IntPtr genericTypes, int typesSize);
        private delegate string TypeGetNameDelegate(int typeHandle);
        private delegate IntPtr TypeGetNamespaceDelegate(int typeHandle);
        private delegate int TypeGetParentDelegate(int typeHandle);
        private delegate int TypeGetFieldDelegate(int typeHandle, string name);
        private delegate int TypeGetPropertyDelegate(int typeHandle, string name);
        private delegate int TypeGetMethodFromArgsCountDelegate(int typeHandle, string name, int argsCount);
        private delegate int TypeToPatchlibTypeDelegate(int typeHandle);
        private delegate bool TypeGetInnerTypesDelegate(int typeHandle, bool includingParent, out IntPtr outArray, out int count);
        private delegate bool TypeGetMethodsDelegate(int typeHandle, bool includingParent, out IntPtr outArray, out int count);
        private delegate bool TypeGetFieldsDelegate(int typeHandle, bool includingParent, out IntPtr outArray, out int count);
        private delegate bool TypeGetPropertiesDelegate(int typeHandle, bool includingParent, out IntPtr outArray, out int count);
        private delegate bool TypeFreeDelegate(int typeHandle);
        private delegate bool ObjectFreeDelegate(int objectHandle);
        private delegate int ObjectPersistDelegate(int objectHandle);
        
        public static void Init()
        {
            RegisterApiMethod<GetTypeDelegate>(Type.GetType, "net_get_type");
            RegisterApiMethod<NewInstanceDelegate>(Type.NewInstance, "net_new_instance");
            RegisterApiMethod<TypeMakeGenericDelegate>(Type.TypeMakeGeneric, "net_type_make_generic");
            RegisterApiMethod<TypeGetNameDelegate>(Type.TypeGetName, "net_type_get_name");
            RegisterApiMethod<TypeGetNamespaceDelegate>(Type.TypeGetNamespace, "net_type_get_namespace");
            RegisterApiMethod<TypeGetParentDelegate>(Type.TypeGetParent, "net_type_get_parent");
            RegisterApiMethod<TypeGetFieldDelegate>(Type.TypeGetField, "net_type_get_field");
            RegisterApiMethod<TypeGetPropertyDelegate>(Type.TypeGetProperty, "net_type_get_property");
            RegisterApiMethod<TypeGetMethodFromArgsCountDelegate>(Type.TypeGetMethodFromArgsCount, "net_type_get_method_from_args_count");
            RegisterApiMethod<TypeToPatchlibTypeDelegate>(Type.TypeToPatchlibType, "net_type_to_patchlib_type");
            RegisterApiMethod<TypeGetInnerTypesDelegate>(Type.TypeGetInnerTypes, "net_type_get_inner_types");
            RegisterApiMethod<TypeGetMethodsDelegate>(Type.TypeGetMethods, "net_type_get_methods");
            RegisterApiMethod<TypeGetFieldsDelegate>(Type.TypeGetFields, "net_type_get_fields");
            RegisterApiMethod<TypeGetPropertiesDelegate>(Type.TypeGetProperties, "net_type_get_properties");
            RegisterApiMethod<TypeFreeDelegate>(Type.TypeFree, "net_type_free");
            RegisterApiMethod<ObjectFreeDelegate>(Type.ObjectFree, "net_object_free");
            RegisterApiMethod<ObjectPersistDelegate>(Type.ObjectPersist, "net_object_persist");
        }
    }

    public static class FieldApi
    {
        // Field API 委托
        private delegate string FieldGetNameDelegate(int fieldHandle);
        private delegate bool FieldIsStaticDelegate(int fieldHandle);
        private delegate bool FieldIsConstDelegate(int fieldHandle);
        private delegate bool FieldIsThreadStaticDelegate(int fieldHandle);
        private delegate bool FieldGetValueDelegate(int fieldHandle, int objectHandle, IntPtr valueOut);
        private delegate bool FieldSetValueDelegate(int fieldHandle, int objectHandle, IntPtr valuePtr);
        private delegate void FieldFreeDelegate(int fieldHandle);

        private delegate int FieldGetTypeDelegate(int fieldHandle);
        
        public static void Init()
        {
            RegisterApiMethod<FieldGetNameDelegate>(Field.GetName, "net_field_get_name");
            RegisterApiMethod<FieldIsStaticDelegate>(Field.IsStatic, "net_field_is_static");
            RegisterApiMethod<FieldIsConstDelegate>(Field.IsConst, "net_field_is_const");
            RegisterApiMethod<FieldIsThreadStaticDelegate>(Field.IsThreadStatic, "net_field_is_thread_static");
            RegisterApiMethod<FieldGetValueDelegate>(Field.GetValue, "net_field_get_value");
            RegisterApiMethod<FieldSetValueDelegate>(Field.SetValue, "net_field_set_value");
            RegisterApiMethod<FieldGetTypeDelegate>(Field.GetType, "net_field_get_type");
            RegisterApiMethod<FieldFreeDelegate>(Field.Free, "net_field_free");
        }
    }

    public static class PropertyApi
    {
        
        // Property API 委托
        private delegate string PropertyGetNameDelegate(int propertyHandle);
        private delegate int PropertyGetGetMethodDelegate(int propertyHandle);
        private delegate int PropertyGetSetMethodDelegate(int propertyHandle);
        private delegate void PropertyFreeDelegate(int propertyHandle);
        
        public static void Init()
        {
            RegisterApiMethod<PropertyGetNameDelegate>(Property.GetName, "net_property_get_name");
            RegisterApiMethod<PropertyGetGetMethodDelegate>(Property.GetGetMethod, "net_property_get_get_method");
            RegisterApiMethod<PropertyGetSetMethodDelegate>(Property.GetSetMethod, "net_property_get_set_method");
            RegisterApiMethod<PropertyFreeDelegate>(Property.Free, "net_property_free");
        }
    }

    public static class MethodInit
    {
        // Method API 委托
        private delegate string MethodGetNameDelegate(int methodHandle);
        private delegate int MethodGetParamCountDelegate(int methodHandle);
        private delegate bool MethodIsInstanceDelegate(int methodHandle);
        private delegate int MethodMakeGenericDelegate(int methodHandle, IntPtr genericTypes, int typesSize);
        private delegate int MethodGetReturnTypeDelegate(int methodHandle);
        private delegate int MethodGetParamDelegate(int methodHandle, int index);
        private delegate string MethodGetParamNameDelegate(int methodHandle, int index);
        private delegate bool MethodInvokeDelegate(int methodHandle, int instanceHandle, int argCount, 
            IntPtr returnValue, IntPtr argsPtr, IntPtr typesPtr);
        private delegate void MethodFreeDelegate(int methodHandle);
        private delegate short HookMethodDelegate(int methodHandle, IntPtr methodSignature, IntPtr prefixHook, IntPtr postfixHook);
        private delegate bool UnHookMethodDelegate(short nodeIndex);
        private delegate bool HasSingleHookNodeDelegate(int methodHandle);
        private delegate bool IsMethodHookedDelegate(int methodHandle);
        private delegate IntPtr GetHookedMethodSigDelegate(int methodHandle);
        private delegate int GetMethodByNodeDelegate(short nodeIndex);
        private delegate int GetMethodTokenDelegate(int methodHandle);
        private delegate int CloneMethodDelegate(int methodHandle);
        
        public static void Init()
        {
            RegisterApiMethod<MethodGetNameDelegate>(Method.GetName, "net_method_get_name");
            RegisterApiMethod<MethodGetParamCountDelegate>(Method.GetParamCount, "net_method_get_param_count");
            RegisterApiMethod<MethodIsInstanceDelegate>(Method.IsInstance, "net_method_is_instance");
            RegisterApiMethod<MethodMakeGenericDelegate>(Method.MakeGeneric, "net_method_make_generic_method");
            RegisterApiMethod<MethodGetReturnTypeDelegate>(Method.GetReturnType, "net_method_get_return_type");
            RegisterApiMethod<MethodGetParamDelegate>(Method.GetParam, "net_method_get_param");
            RegisterApiMethod<MethodGetParamNameDelegate>(Method.GetParamName, "net_method_get_param_name");
            RegisterApiMethod<MethodInvokeDelegate>(Method.Invoke, "net_method_invoke");
            RegisterApiMethod<MethodFreeDelegate>(Method.Free, "net_method_free");
            RegisterApiMethod<HookMethodDelegate>(Method.HookMethod, "net_hook_method");
            RegisterApiMethod<UnHookMethodDelegate>(Method.UnHookMethod, "net_unhook_method");
            RegisterApiMethod<HasSingleHookNodeDelegate>(Method.HasSingleHookNode, "net_has_single_hook_node");
            RegisterApiMethod<IsMethodHookedDelegate>(Method.IsMethodHooked, "net_is_method_hooked");
            RegisterApiMethod<GetHookedMethodSigDelegate>(Method.GetHookedMethodSig, "net_get_hooked_method_sig");
            RegisterApiMethod<GetMethodByNodeDelegate>(Method.GetMethodByNode, "net_get_method_by_hook_node");
            RegisterApiMethod<GetMethodTokenDelegate>(Method.GetMethodToken, "net_method_get_token");
            RegisterApiMethod<CloneMethodDelegate>(Method.CloneMethod, "net_method_clone");
        }
    }

    public static class StructInit
    {
        public static void InitAll()
        {
            ArrayInit.Init();
            StringInit.Init();
            ListInit.Init();
            DictionaryInit.Init();
        }

        private static class ArrayInit
        {
            // Array 委托
            private delegate int ArrayCreateDelegate(int size, int type);
            private delegate bool ArrayAtDelegate(int arrayHandle, int index, IntPtr outValue);
            private delegate bool ArraySetDelegate(int arrayHandle, int index, IntPtr valuePtr, Type.PatchType type);
            private delegate bool ArrayFillDelegate(int arrayHandle, IntPtr valuePtr, Type.PatchType type);
            private delegate int ArrayLengthDelegate(int arrayHandle);
            private delegate bool ArrayClearDelegate(int arrayHandle);
            
            public static void Init()
            {
                RegisterApiMethod<ArrayCreateDelegate>(Struct.Array.Create, "net_array_create");
                RegisterApiMethod<ArrayAtDelegate>(Struct.Array.At, "net_array_at");
                RegisterApiMethod<ArraySetDelegate>(Struct.Array.Set, "net_array_set");
                RegisterApiMethod<ArrayFillDelegate>(Struct.Array.Fill, "net_array_fill");
                RegisterApiMethod<ArrayLengthDelegate>(Struct.Array.Length, "net_array_length");
                RegisterApiMethod<ArrayClearDelegate>(Struct.Array.Clear, "net_array_clear");
            }
        }
        
        private static class StringInit
        {
            
            // String 委托
            private delegate int StringCreateDelegate(IntPtr strPtr, int length);
            private delegate IntPtr StringCStr16Delegate(int strHandle);
            private delegate IntPtr StringCStrDelegate(int strHandle);
            private delegate bool StringEmptyDelegate(int strHandle);
            private delegate int StringLengthDelegate(int strHandle);

            
            public static void Init()
            {
                RegisterApiMethod<StringCreateDelegate>(Struct.String.Create, "net_string_create");
                RegisterApiMethod<StringCStr16Delegate>(Struct.String.CStr16, "net_string_cstr16");
                RegisterApiMethod<StringCStrDelegate>(Struct.String.CStr, "net_string_cstr");
                RegisterApiMethod<StringEmptyDelegate>(Struct.String.Empty, "net_string_empty");
                RegisterApiMethod<StringLengthDelegate>(Struct.String.Length, "net_string_length");
            }
        }

        private static class ListInit
        {
            
            // List 委托
            private delegate int ListCreateDelegate(int capacity, int type);
            private delegate bool ListCopyFromDelegate(int listHandle, int arrayHandle);
            private delegate bool ListAddDelegate(int listHandle, IntPtr valuePtr);
            private delegate bool ListRemoveDelegate(int listHandle, IntPtr valuePtr);
            private delegate bool ListRemoveAtDelegate(int listHandle, int index);
            private delegate bool ListClearDelegate(int listHandle);
            private delegate int ListGetArrayDelegate(int listHandle);
            
            public static void Init()
            {
                RegisterApiMethod<ListCreateDelegate>(Struct.List.Create, "net_list_create");
                RegisterApiMethod<ListCopyFromDelegate>(Struct.List.CopyFrom, "net_list_copy_from");
                RegisterApiMethod<ListAddDelegate>(Struct.List.Add, "net_list_add");
                RegisterApiMethod<ListRemoveDelegate>(Struct.List.Remove, "net_list_remove");
                RegisterApiMethod<ListRemoveAtDelegate>(Struct.List.RemoveAt, "net_list_remove_at");
                RegisterApiMethod<ListClearDelegate>(Struct.List.Clear, "net_list_clear");
                RegisterApiMethod<ListGetArrayDelegate>(Struct.List.GetArray, "net_list_get_array");
            }
        }
        
        private static class DictionaryInit
        {
            
            // Dictionary 委托
            private delegate int DictionaryCreateDelegate(int keyTypeHandle, int valueTypeHandle, int capacity);
            private delegate bool DictionaryAddDelegate(int dictionaryHandle, IntPtr keyPtr, IntPtr valuePtr);
            private delegate bool DictionaryGetValueDelegate(int dictionaryHandle, IntPtr keyPtr, IntPtr outValuePtr);
            private delegate bool DictionarySetValueDelegate(int dictionaryHandle, IntPtr keyPtr, IntPtr valuePtr);
            private delegate bool DictionaryClearDelegate(int dictionaryHandle);
            private delegate int DictionaryLengthDelegate(int dictionaryHandle);
            private delegate bool DictionaryRemoveDelegate(int dictionaryHandle, IntPtr keyPtr);

            
            public static void Init()
            {
                RegisterApiMethod<DictionaryCreateDelegate>(Struct.Dictionary.Create, "net_dictionary_create");
                RegisterApiMethod<DictionaryAddDelegate>(Struct.Dictionary.Add, "net_dictionary_add");
                RegisterApiMethod<DictionaryGetValueDelegate>(Struct.Dictionary.GetValue, "net_dictionary_get_value");
                RegisterApiMethod<DictionarySetValueDelegate>(Struct.Dictionary.SetValue, "net_dictionary_set_value");
                RegisterApiMethod<DictionaryClearDelegate>(Struct.Dictionary.Clear, "net_dictionary_clear");
                RegisterApiMethod<DictionaryLengthDelegate>(Struct.Dictionary.Length, "net_dictionary_length");
                RegisterApiMethod<DictionaryRemoveDelegate>(Struct.Dictionary.Remove, "net_dictionary_remove");
            }
        }
    }

    // 主初始化方法
    public static void InitializeAllApis()
    {
        Logger.Info("Starting API initialization...");
        
        TypeApi.Init();
        FieldApi.Init();
        PropertyApi.Init();
        MethodInit.Init();
        StructInit.InitAll();
        
        Logger.Info("All APIs initialized successfully");
    }
}