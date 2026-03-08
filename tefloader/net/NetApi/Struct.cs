// /*******************************************************************************
//  * tefkernel - Struct.cs
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
//  * Created: 2026/01/10
//  *******************************************************************************/

using System.Runtime.InteropServices;
using System.Text;

namespace tefloader.NetApi;

public static class Struct
{
    public static class Array
    {
        public static int Create(int size, int type)
        {
            var array = System.Array.CreateInstance(Asset.Types[type], size);
            return Asset.Objects.Add(array);
        }

        public static bool At(int arrayHandle, int index, IntPtr outValue)
        {
            var array = Asset.Objects[arrayHandle] as System.Array;
            var value = array?.GetValue(index);
            
            if (value != null)
                Utils.SetNativeValue(outValue, value);

            return true;
        }

        public static bool Set(int arrayHandle, int index, IntPtr valuePtr, Type.PatchType type)
        {
            var array = Asset.Objects[arrayHandle] as System.Array;
            var value = Utils.GetNativeValue(valuePtr, Type.TypeMapping[type]);

            array?.SetValue(value, index);

            return true;
        }

        public static bool Fill(int arrayHandle, IntPtr valuePtr, Type.PatchType type)
        {
            var array = Asset.Objects[arrayHandle] as System.Array;
            var value = Utils.GetNativeValue(valuePtr, Type.TypeMapping[type]);
            
            if (array != null) Parallel.For(0, array.Length, i => array.SetValue(value, i));
            
            return true;
        }

        public static int Length(int arrayHandle)
        {
            var array = Asset.Objects[arrayHandle] as System.Array;
            return array?.Length ?? 0;
        }

        public static bool Clear(int arrayHandle)
        {
            if (Asset.Objects[arrayHandle] is System.Array array) System.Array.Clear(array, 0, array.Length);
            return true;
        }
    }
    
    public static class String
    {
        public static int Create(IntPtr strPtr, int length)
        {
            var str = length > 0 ? Marshal.PtrToStringAnsi(strPtr, length) : string.Empty;
            return Asset.Objects.Add(str);
        }

        public static IntPtr CStr16(int strHandle)
        {
            var str = Asset.Objects[strHandle] as string;
            if (string.IsNullOrEmpty(str))
                return IntPtr.Zero;
                
            var utf16Bytes = Encoding.Unicode.GetBytes(str);
            var ptr = Marshal.AllocHGlobal(utf16Bytes.Length);
            Marshal.Copy(utf16Bytes, 0, ptr, utf16Bytes.Length);
            return ptr;
        }

        public static IntPtr CStr(int strHandle)
        {
            var str = Asset.Objects[strHandle] as string;
            if (str == null)
                return IntPtr.Zero;
                
            var utf8Bytes = Encoding.UTF8.GetBytes(str);
            var ptr = Marshal.AllocHGlobal(utf8Bytes.Length + 1);
            Marshal.Copy(utf8Bytes, 0, ptr, utf8Bytes.Length);
            Marshal.WriteByte(ptr, utf8Bytes.Length, 0); // 添加null终止符
            return ptr;
        }

        public static bool Empty(int strHandle)
        {
            var str = Asset.Objects[strHandle] as string;
            return string.IsNullOrEmpty(str);
        }

        public static int Length(int strHandle)
        {
            var str = Asset.Objects[strHandle] as string;
            return str?.Length ?? 0;
        }
    }
    
    public static class List
    {
        public static int Create(int capacity, int type)
        {
            var elementType = Asset.Types[type];
            var listType = typeof(List<>).MakeGenericType(elementType);
            var list = Activator.CreateInstance(listType, capacity);
            return Asset.Objects.Add(list);
        }

        public static bool CopyFrom(int listHandle, int arrayHandle)
        {
            var list = Asset.Objects[listHandle];
            var array = Asset.Objects[arrayHandle] as System.Array;
            
            if (array == null)
                return false;
                
            try
            {
                var addRangeMethod = list.GetType().GetMethod("AddRange");
                if (addRangeMethod != null)
                {
                    addRangeMethod.Invoke(list, [array]);
                    return true;
                }
            }
            catch
            {
                return false;
            }
            
            return false;
        }

        public static bool Add(int listHandle, IntPtr valuePtr)
        {
            var list = Asset.Objects[listHandle];

            try
            {
                var listType = list.GetType();
                var elementType = listType.GetGenericArguments()[0];
                
                // 获取实际类型

                var value = Utils.GetNativeValue(valuePtr, elementType);
                var addMethod = listType.GetMethod("Add");
                addMethod?.Invoke(list, [value]);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static bool Remove(int listHandle, IntPtr valuePtr)
        {
            var list = Asset.Objects[listHandle];

            try
            {
                var listType = list.GetType();
                var elementType = listType.GetGenericArguments()[0];
                
                // 获取实际类型

                var value = Utils.GetNativeValue(valuePtr, elementType);
                var removeMethod = listType.GetMethod("Remove");
                var result = removeMethod?.Invoke(list, [value]);
                return result is bool b ? b : (bool?)result ?? false;
            }
            catch
            {
                return false;
            }
        }

        public static bool RemoveAt(int listHandle, int index)
        {
            var list = Asset.Objects[listHandle];

            try
            {
                var listType = list.GetType();
                var removeAtMethod = listType.GetMethod("RemoveAt");
                removeAtMethod?.Invoke(list, [index]);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static bool Clear(int listHandle)
        {
            var list = Asset.Objects[listHandle];

            try
            {
                var listType = list.GetType();
                var clearMethod = listType.GetMethod("Clear");
                clearMethod?.Invoke(list, null);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static int GetArray(int listHandle)
        {
            var list = Asset.Objects[listHandle];

            try
            {
                var listType = list.GetType();
                var elementType = listType.GetGenericArguments()[0];
                
                // 通过反射获取Count属性
                var countProperty = listType.GetProperty("Count");
                if (countProperty != null)
                {
                    var count = (int)countProperty.GetValue(list);
                
                    // 创建数组
                    var array = System.Array.CreateInstance(elementType, count);
                
                    // 通过反射调用CopyTo方法
                    var copyToMethod = listType.GetMethod("CopyTo", [typeof(System.Array), typeof(int)]);
                    copyToMethod?.Invoke(list, [array, 0]);
                
                    return Asset.Objects.Add(array);
                }

                return -1;
            }
            catch
            {
                return -1;
            }
        }
    }
    
    public static class Dictionary
    {
        public static int Create(int keyTypeHandle, int valueTypeHandle, int capacity)
        {
            var keyType = Asset.Types[keyTypeHandle];
            var valueType = Asset.Types[valueTypeHandle];

            var dictType = typeof(Dictionary<,>).MakeGenericType(keyType, valueType);
            var dictionary = Activator.CreateInstance(dictType, capacity);
            
            return Asset.Objects.Add(dictionary);
        }

        public static bool Add(int dictionaryHandle, IntPtr keyPtr, IntPtr valuePtr)
        {
            var dictionary = Asset.Objects[dictionaryHandle];

            try
            {
                var dictType = dictionary.GetType();
                var genericArgs = dictType.GetGenericArguments();
                var keyType = genericArgs[0];
                var valueType = genericArgs[1];
                
                // 获取键值
                var key = Utils.GetNativeValue(keyPtr, keyType);
                var value = Utils.GetNativeValue(valuePtr, valueType);
                
                var addMethod = dictType.GetMethod("Add");
                addMethod?.Invoke(dictionary, new[] { key, value });
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static bool GetValue(int dictionaryHandle, IntPtr keyPtr, IntPtr outValuePtr)
        {
            var dictionary = Asset.Objects[dictionaryHandle];
            if (outValuePtr == IntPtr.Zero)
                return false;
                
            try
            {
                var dictType = dictionary.GetType();
                var genericArgs = dictType.GetGenericArguments();
                var keyType = genericArgs[0];

                // 获取键
                var key = Utils.GetNativeValue(keyPtr, keyType);
                
                // 尝试获取值
                var tryGetValueMethod = dictType.GetMethod("TryGetValue");
                var parameters = new[] { key, null };
                var result = (bool)(tryGetValueMethod?.Invoke(dictionary, parameters) ?? false);

                if (!result) return false;
                // 将值写入输出指针
                var p = parameters[1];
                if (p != null)
                    Utils.SetNativeValue(outValuePtr, p);
                return true;

            }
            catch
            {
                return false;
            }
        }

        public static bool SetValue(int dictionaryHandle, IntPtr keyPtr, IntPtr valuePtr)
        {
            var dictionary = Asset.Objects[dictionaryHandle];

            try
            {
                var dictType = dictionary.GetType();
                var genericArgs = dictType.GetGenericArguments();
                var keyType = genericArgs[0];
                var valueType = genericArgs[1];
                
                // 获取键值
                var key = Utils.GetNativeValue(keyPtr, keyType);
                var value = Utils.GetNativeValue(valuePtr, valueType);
                
                var indexer = dictType.GetProperty("Item");
                indexer?.SetValue(dictionary, value, [key]);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static bool Clear(int dictionaryHandle)
        {
            var dictionary = Asset.Objects[dictionaryHandle];

            try
            {
                var dictType = dictionary.GetType();
                var clearMethod = dictType.GetMethod("Clear");
                clearMethod?.Invoke(dictionary, null);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static int Length(int dictionaryHandle)
        {
            var dictionary = Asset.Objects[dictionaryHandle];

            try
            {
                var dictType = dictionary.GetType();
                var countProperty = dictType.GetProperty("Count");
                return (int)(countProperty?.GetValue(dictionary) ?? 0);
            }
            catch
            {
                return 0;
            }
        }

        public static bool Remove(int dictionaryHandle, IntPtr keyPtr)
        {
            var dictionary = Asset.Objects[dictionaryHandle];

            try
            {
                var dictType = dictionary.GetType();
                var genericArgs = dictType.GetGenericArguments();
                var keyType = genericArgs[0];
                
                // 获取键
                var key = Utils.GetNativeValue(keyPtr, keyType);
                
                var removeMethod = dictType.GetMethod("Remove");
                var result = removeMethod?.Invoke(dictionary, [key]);
                return result is bool b ? b : (bool?)result ?? false;
            }
            catch
            {
                return false;
            }
        }
    }
}