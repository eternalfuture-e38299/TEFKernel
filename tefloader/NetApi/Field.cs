// /*******************************************************************************
//  * tefkernel - Field.cs
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

namespace tefloader.NetApi;

public static class Field
{
    public static string GetName(int fieldHandle)
    {
        return Asset.FieldInfos[fieldHandle].Name;
    }

    public static bool IsStatic(int fieldHandle)
    {
        return Asset.FieldInfos[fieldHandle].IsStatic;
    }

    public static bool IsConst(int fieldHandle)
    {
        return Asset.FieldInfos[fieldHandle].IsLiteral;
    }

    public static bool IsThreadStatic(int fieldHandle)
    {
        return Attribute.IsDefined(Asset.FieldInfos[fieldHandle], typeof(ThreadStaticAttribute));
    }

    public static bool GetValue(int fieldHandle, int objectHandle, IntPtr valueOut)
    {
        try
        {
            var fieldInfo = Asset.FieldInfos[fieldHandle];

            object? instance = null;
            if (objectHandle != -1) instance = Asset.Objects[objectHandle];

            object? value;
            if (fieldInfo.IsLiteral)
            {
                value = fieldInfo.GetRawConstantValue();
            }
            else
            {
                if (!fieldInfo.IsStatic && instance == null) return false;
                value = fieldInfo.GetValue(instance);
            }

            return Utils.SetNativeValue(valueOut, value);
        }
        catch (Exception ex)
        {
            Logger.Error($"GetValue failed: {ex.Message}");
            return false;
        }
    }

    public static bool SetValue(int fieldHandle, int objectHandle, IntPtr valuePtr)
    {
        try
        {
            var fieldInfo = Asset.FieldInfos[fieldHandle];

            if (fieldInfo.IsInitOnly || fieldInfo.IsLiteral) return false; // 只读字段或常量

            object? instance = null;
            if (objectHandle != -1) instance = Asset.Objects[objectHandle];

            // 检查实例是否为null（对于非静态字段）
            if (!fieldInfo.IsStatic && instance == null) return false;

            // 从native指针获取值
            var value = Utils.GetNativeValue(valuePtr, fieldInfo.FieldType);

            // 设置字段值
            fieldInfo.SetValue(instance, value);

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"SetValue failed: {ex.Message}");
            return false;
        }
    }

    public static int GetType(int fieldHandle)
    {
        return Utils.GetPatchTypeByType(Asset.FieldInfos[fieldHandle].FieldType);
    }

    public static void Free(int fieldHandle)
    {
        Asset.FieldInfos.RemoveAt(fieldHandle);
    }
}