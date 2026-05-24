// /*******************************************************************************
//  * tefloader - Initialization.cs
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

using System.Runtime.InteropServices;

namespace tefloader.Il2CppApi;

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
        Logger.Debug($"Successfully registered {name}");
    }

    public static void Cleanup()
    {
        KeepAlivePointers.Clear();
        KeepAliveDelegates.Clear();
    }

    /// <summary>
    /// 一键注册所有 IL2CPP API
    /// </summary>
    public static void RegisterAllApis()
    {
        BasicApi.Init();
        ClassApi.Init();
        MethodApi.Init();
        PropertyApi.Init();
        FieldApi.Init();
        StringApi.Init();
        ArrayApi.Init();
    }
    
    public static class BasicApi
    {
        public static unsafe void Init()
        {
            RegisterApiMethod<DelegateIl2CppDomainGet>(Basic.il2cpp_domain_get, "il2cpp_domain_get");
            RegisterApiMethod<DelegateIl2CppDomainGetAssemblies>(Basic.il2cpp_domain_get_assemblies,
                "il2cpp_domain_get_assemblies");
            RegisterApiMethod<DelegateIl2CppGetCorlib>(Basic.il2cpp_get_corlib, "il2cpp_get_corlib");
            RegisterApiMethod<DelegateIl2CppFree>(Basic.il2cpp_free, "il2cpp_free");
            RegisterApiMethod<DelegateIl2CppObjectCopy>(Basic.il2cpp_object_copy, "il2cpp_object_copy");
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppDomainGet();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr* DelegateIl2CppDomainGetAssemblies(IntPtr domainPtr, out int size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppGetCorlib();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void DelegateIl2CppFree(IntPtr obj);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppObjectCopy(IntPtr sourcePtr);
    }

    public static class ClassApi
    {
        public static unsafe void Init()
        {
            // Register all Class API methods
            RegisterApiMethod<DelegateIl2CppClassFromName>(Class.il2cpp_class_from_name, "il2cpp_class_from_name");
            RegisterApiMethod<DelegateIl2CppClassGetNestedTypes>(Class.il2cpp_class_get_nested_types,
                "il2cpp_class_get_nested_types");
            RegisterApiMethod<DelegateIl2CppClassGetName>(Class.il2cpp_class_get_name, "il2cpp_class_get_name");
            RegisterApiMethod<DelegateIl2CppClassGetParent>(Class.il2cpp_class_get_parent, "il2cpp_class_get_parent");
            RegisterApiMethod<DelegateIl2CppClassGetMethods>(Class.il2cpp_class_get_methods,
                "il2cpp_class_get_methods");
            RegisterApiMethod<DelegateIl2CppClassGetFields>(Class.il2cpp_class_get_fields, "il2cpp_class_get_fields");
            RegisterApiMethod<DelegateIl2CppClassGetProperties>(Class.il2cpp_class_get_properties,
                "il2cpp_class_get_properties");
            RegisterApiMethod<DelegateIl2CppClassGetNamespace>(Class.il2cpp_class_get_namespace,
                "il2cpp_class_get_namespace");
            RegisterApiMethod<DelegateIl2CppClassIsAbstract>(Class.il2cpp_class_is_abstract,
                "il2cpp_class_is_abstract");
            RegisterApiMethod<DelegateIl2CppClassIsInterface>(Class.il2cpp_class_is_interface,
                "il2cpp_class_is_interface");
            RegisterApiMethod<DelegateIl2CppClassIsEnum>(Class.il2cpp_class_is_enum, "il2cpp_class_is_enum");
            RegisterApiMethod<DelegateIl2CppClassIsGeneric>(Class.il2cpp_class_is_generic, "il2cpp_class_is_generic");
            RegisterApiMethod<DelegateIl2CppClassGetFieldFromName>(Class.il2cpp_class_get_field_from_name,
                "il2cpp_class_get_field_from_name");
            RegisterApiMethod<DelegateIl2CppClassGetPropertyFromName>(Class.il2cpp_class_get_property_from_name,
                "il2cpp_class_get_property_from_name");
            RegisterApiMethod<DelegateIl2CppClassGetMethodFromName>(Class.il2cpp_class_get_method_from_name,
                "il2cpp_class_get_method_from_name");
            RegisterApiMethod<DelegateIl2CppObjectNew>(Class.il2cpp_object_new, "il2cpp_object_new");
            RegisterApiMethod<DelegateIl2CppClassMakeGeneric>(Class.il2cpp_class_make_generic,
                "il2cpp_class_make_generic");
            RegisterApiMethod<DelegateIl2CppClassIsSame>(Class.il2cpp_class_is_same, "il2cpp_class_is_same");
        }

        // Delegates for Class API methods
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppClassFromName(IntPtr imagePtr, string namespaze, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr* DelegateIl2CppClassGetNestedTypes(IntPtr classPtr, out int size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string DelegateIl2CppClassGetName(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppClassGetParent(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr* DelegateIl2CppClassGetMethods(IntPtr classPtr, out int size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr* DelegateIl2CppClassGetFields(IntPtr classPtr, out int size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr* DelegateIl2CppClassGetProperties(IntPtr classPtr, out int size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string? DelegateIl2CppClassGetNamespace(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppClassIsAbstract(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppClassIsInterface(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppClassIsEnum(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppClassIsGeneric(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppClassGetFieldFromName(IntPtr classPtr, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppClassGetPropertyFromName(IntPtr classPtr, string name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppClassGetMethodFromName(IntPtr classPtr, string name, int argsCount);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppObjectNew(IntPtr classPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr
            DelegateIl2CppClassMakeGeneric(IntPtr classPtr, IntPtr* typesPtr, int typesCount);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppClassIsSame(IntPtr classPtr1, IntPtr classPtr2);
    }

    public static class MethodApi
    {
        public static unsafe void Init()
        {
            // Register all Method API methods
            RegisterApiMethod<DelegateIl2CppMethodGetName>(Method.il2cpp_method_get_name, "il2cpp_method_get_name");
            RegisterApiMethod<DelegateIl2CppMethodGetParamCount>(Method.il2cpp_method_get_param_count,
                "il2cpp_method_get_param_count");
            RegisterApiMethod<DelegateIl2CppMethodGetParamName>(Method.il2cpp_method_get_param_name,
                "il2cpp_method_get_param_name");
            RegisterApiMethod<DelegateIl2CppMethodGetParam>(Method.il2cpp_method_get_param, "il2cpp_method_get_param");
            RegisterApiMethod<DelegateIl2CppMethodIsInstance>(Method.il2cpp_method_is_instance,
                "il2cpp_method_is_instance");
            RegisterApiMethod<DelegateIl2CppMethodIsGeneric>(Method.il2cpp_method_is_generic,
                "il2cpp_method_is_generic");
            RegisterApiMethod<DelegateIl2CppMethodGetReturnType>(Method.il2cpp_method_get_return_type,
                "il2cpp_method_get_return_type");
            RegisterApiMethod<DelegateIl2CppMethodGetDeclaringType>(Method.il2cpp_method_get_declaring_type,
                "il2cpp_method_get_declaring_type");
            RegisterApiMethod<DelegateIl2CppMethodGetClass>(Method.il2cpp_method_get_class, "il2cpp_method_get_class");
            RegisterApiMethod<DelegateIl2CppMethodGetObject>(Method.il2cpp_method_get_object,
                "il2cpp_method_get_object");
            RegisterApiMethod<DelegateIl2CppMethodGetToken>(Method.il2cpp_method_get_token, "il2cpp_method_get_token");
            RegisterApiMethod<DelegateIl2CppMethodInvoke>(Method.il2cpp_method_invoke, "il2cpp_method_invoke");
            RegisterApiMethod<DelegateIl2cppMethodMakeGeneric>(Method.il2cpp_method_make_generic,
                "il2cpp_method_make_generic");
        }

        // Delegates for Method API methods
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string? DelegateIl2CppMethodGetName(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate uint DelegateIl2CppMethodGetParamCount(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string? DelegateIl2CppMethodGetParamName(IntPtr methodPtr, uint index);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppMethodGetParam(IntPtr methodPtr, uint index);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppMethodIsInstance(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppMethodIsGeneric(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppMethodGetReturnType(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppMethodGetDeclaringType(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppMethodGetClass(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppMethodGetObject(IntPtr methodPtr, IntPtr refclassPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate uint DelegateIl2CppMethodGetToken(IntPtr methodPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppMethodInvoke(IntPtr methodPtr, IntPtr objPtr, IntPtr* paramsPtr,
            IntPtr returnValuePtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr DelegateIl2cppMethodMakeGeneric(IntPtr methodPtr, IntPtr* typesPtr,
            int typesCount);
    }

    public static class PropertyApi
    {
        public static void Init()
        {
            // Register all Property API methods
            RegisterApiMethod<DelegateIl2CppPropertyGetName>(Property.il2cpp_property_get_name,
                "il2cpp_property_get_name");
            RegisterApiMethod<DelegateIl2CppPropertyGetGetMethod>(Property.il2cpp_property_get_get_method,
                "il2cpp_property_get_get_method");
            RegisterApiMethod<DelegateIl2CppPropertyGetSetMethod>(Property.il2cpp_property_get_set_method,
                "il2cpp_property_get_set_method");
        }

        // Delegates for Property API methods
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string? DelegateIl2CppPropertyGetName(IntPtr propertyPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppPropertyGetGetMethod(IntPtr propertyPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppPropertyGetSetMethod(IntPtr propertyPtr);
    }

    public static class FieldApi
    {
        public static void Init()
        {
            // Register all Field API methods
            RegisterApiMethod<DelegateIl2CppFieldGetName>(Field.il2cpp_field_get_name, "il2cpp_field_get_name");
            RegisterApiMethod<DelegateIl2CppFieldGetParent>(Field.il2cpp_field_get_parent, "il2cpp_field_get_parent");
            RegisterApiMethod<DelegateIl2CppFieldGetType>(Field.il2cpp_field_get_type, "il2cpp_field_get_type");

            RegisterApiMethod<DelegateIl2CppFieldGetValue>(Field.il2cpp_field_get_value, "il2cpp_field_get_value");
            RegisterApiMethod<DelegateIl2CppFieldSetValue>(Field.il2cpp_field_set_value, "il2cpp_field_set_value");
            RegisterApiMethod<DelegateIl2CppFieldIsStatic>(Field.il2cpp_field_is_static, "il2cpp_field_is_static");
            RegisterApiMethod<DelegateIl2CppFieldIsThreadStatic>(Field.il2cpp_field_is_thread_static,
                "il2cpp_field_is_thread_static");
            RegisterApiMethod<DelegateIl2CppFieldIsLiteral>(Field.il2cpp_field_is_literal, "il2cpp_field_is_literal");
        }

        // Delegates for Field API methods
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate string? DelegateIl2CppFieldGetName(IntPtr fieldPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppFieldGetParent(IntPtr fieldPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppFieldGetType(IntPtr fieldPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void DelegateIl2CppFieldGetValue(IntPtr fieldPtr, IntPtr objPtr, IntPtr returnValuePtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppFieldSetValue(IntPtr fieldPtr, IntPtr objPtr, IntPtr valuePtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppFieldIsStatic(IntPtr fieldPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppFieldIsThreadStatic(IntPtr fieldPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate bool DelegateIl2CppFieldIsLiteral(IntPtr fieldPtr);
    }

    public static class StringApi
    {
        public static unsafe void Init()
        {
            // Register all String API methods
            RegisterApiMethod<DelegateIl2CppStringLength>(String.il2cpp_string_length, "il2cpp_string_length");
            RegisterApiMethod<DelegateIl2CppStringNew>(String.il2cpp_string_new, "il2cpp_string_new");
            RegisterApiMethod<DelegateIl2CppStringCStr>(String.il2cpp_string_cstr, "il2cpp_string_cstr");
        }

        // Delegates for String API methods
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DelegateIl2CppStringLength(IntPtr strPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate IntPtr DelegateIl2CppStringNew(byte* cstrPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate byte* DelegateIl2CppStringCStr(IntPtr strPtr);
    }

    public static class ArrayApi
    {
        public static unsafe void Init()
        {
            // Register all Array API methods
            RegisterApiMethod<DelegateIl2CppArrayNew>(Array.il2cpp_array_new, "il2cpp_array_new");
            RegisterApiMethod<DelegateIl2CppArrayElementSize>(Array.il2cpp_array_element_size,
                "il2cpp_array_element_size");
            RegisterApiMethod<DelegateIl2CppArrayLength>(Array.il2cpp_array_length, "il2cpp_array_length");
            RegisterApiMethod<DelegateIl2CppArrayAt>(Array.il2cpp_array_at, "il2cpp_array_at");
            RegisterApiMethod<DelegateIl2CppArraySet>(Array.il2cpp_array_set, "il2cpp_array_set");
            RegisterApiMethod<DelegateIl2CppArrayFill>(Array.il2cpp_array_fill, "il2cpp_array_fill");
            RegisterApiMethod<DelegateIl2CppArrayCopyFromC>(Array.il2cpp_array_copy_from_c, "il2cpp_array_copy_from_c");
            RegisterApiMethod<DelegateIl2CppArrayCopyToC>(Array.il2cpp_array_copy_to_c, "il2cpp_array_copy_to_c");
        }

        // Delegates
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr DelegateIl2CppArrayNew(IntPtr elementTypeInfo, int length);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DelegateIl2CppArrayElementSize(IntPtr arrayPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DelegateIl2CppArrayLength(IntPtr arrayPtr);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppArrayAt(IntPtr arrayPtr, int index, void* outValue);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppArraySet(IntPtr arrayPtr, int index, void* value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppArrayFill(IntPtr arrayPtr, void* value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppArrayCopyFromC(IntPtr destArrayPtr, void* src, int count);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private unsafe delegate bool DelegateIl2CppArrayCopyToC(void* dest, IntPtr srcArrayPtr, int count);
    }
}