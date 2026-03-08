// /*******************************************************************************
//  * tefkernel - Asset.cs
//  * Copyright (C) 2025 eternalfuture-e38299
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
//  * Created: 2025/11/23
//  *******************************************************************************/

using System.Reflection;

namespace tefloader.NetApi;

public static class Asset
{
    public static readonly ReusableList<System.Type> Types = [];
    public static readonly ReusableList<MemberInfo> MethodInfos = []; // 支持构造函数与普通函数
    public static readonly ReusableList<FieldInfo> FieldInfos = [];
    public static readonly ReusableList<PropertyInfo> PropertyInfos = [];
    public static readonly ReusableList<object> Objects = [];
}