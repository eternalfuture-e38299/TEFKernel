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
    public static string GetName(int fieldHandle) => Asset.FieldInfos[fieldHandle].Name;

    public static bool IsStatic(int fieldHandle) => Asset.FieldInfos[fieldHandle].IsStatic;

    public static bool IsConst(int fieldHandle) => Asset.FieldInfos[fieldHandle].IsLiteral;

    public static bool IsThreadStatic(int fieldHandle) => Attribute.IsDefined(Asset.FieldInfos[fieldHandle], typeof(ThreadStaticAttribute));

    public static bool GetValue(int fieldHandle, int objectHandle, IntPtr valueOut)
    {
        var fieldInfo = Asset.FieldInfos[fieldHandle];
        var instance = objectHandle == -1 ? null : Asset.Objects[objectHandle];
        var value = fieldInfo.IsLiteral ? fieldInfo.GetRawConstantValue() : fieldInfo.GetValue(instance);

        return Utils.SetNativeValue(valueOut, value);
    }

    public static bool SetValue(int fieldHandle, int objectHandle, IntPtr valuePtr)
    {
        var fieldInfo = Asset.FieldInfos[fieldHandle];
        var instance = objectHandle == -1 ? null : Asset.Objects[objectHandle];
        var value = Utils.GetNativeValue(valuePtr, fieldInfo.GetType());

        fieldInfo.SetValue(instance, value);

        return true;
    }

    public static void Free(int fieldHandle) => Asset.FieldInfos.RemoveAt(fieldHandle);
}