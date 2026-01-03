// /*******************************************************************************
//  * tefkernel - Property.cs
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

public static class Property
{
    public static string GetName(int propertyHandle) => Asset.PropertyInfos[propertyHandle].Name;

    public static int GetGetMethod(int propertyHandle)
    {
        var propertyInfo = Asset.PropertyInfos[propertyHandle];
        return Asset.MethodInfos.Add(propertyInfo.GetMethod);
    }
    
    public static int GetSetMethod(int propertyHandle)
    {
        var propertyInfo = Asset.PropertyInfos[propertyHandle];
        return Asset.MethodInfos.Add(propertyInfo.SetMethod);
    }

    public static void Free(int propertyHandle) => Asset.PropertyInfos.RemoveAt(propertyHandle);
}