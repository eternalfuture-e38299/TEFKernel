// /*******************************************************************************
//  * tefkernel - Type.cs
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

using System.Collections.Concurrent;
using System.Reflection;

namespace tefloader.NetApi;

public class Type
{
    public static int GetType(string ns, string name)
    {
        var fullName = string.IsNullOrEmpty(ns) ? name : $"{ns}.{name}";
        foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
        {
            var type = assembly.GetType(fullName);
            if (type != null) return Asset.Types.Add(type);
        }

        return -1;
    }
    
    public static int GetNestedTypes(int type, ref int iter)
    {
        if (!MemberCache<System.Type>.Cache.TryGetValue(type, out var cache))
        {
            var nestedTypes = Asset.Types[type].GetNestedTypes(
                BindingFlags.Public | BindingFlags.NonPublic |
                BindingFlags.Static | BindingFlags.Instance);
            cache = new MemberCacheItem<System.Type>(nestedTypes);
            MemberCache<System.Type>.Cache[type] = cache;
        }

        if (iter >= cache.Members.Length)
        {
            MemberCache<System.Type>.Cache.TryRemove(type, out _);
            return -1;
        }

        var inner = cache.Members[iter];
        iter++;

        return Asset.Types.Add(inner);
    }
    
    
    

    public class MemberCacheItem<T>(T[] members)
        where T : MemberInfo
    {
        public T[] Members { get; } = members;
        public int Iter { get; set; } = 0;
    }

    private static class MemberCache<T> where T : MemberInfo
    {
        public static readonly ConcurrentDictionary<int, MemberCacheItem<T>> Cache = new();
    }
}