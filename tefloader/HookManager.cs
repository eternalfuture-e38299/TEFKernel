/*******************************************************************************
 * tefkernel - HookManager.cs
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

using System.Reflection;
using System.Runtime.InteropServices;
using HarmonyLib;
using tefloader.NetApi;
using Type = System.Type;

namespace tefloader;

public static unsafe class HookManager
{
    // 基础类型映射
    private static readonly Dictionary<Type, int> BasicTypeSizes = new()
    {
        { typeof(bool), sizeof(bool) },
        { typeof(byte), sizeof(byte) },
        { typeof(sbyte), sizeof(sbyte) },
        { typeof(short), sizeof(short) },
        { typeof(ushort), sizeof(ushort) },
        { typeof(int), sizeof(int) },
        { typeof(uint), sizeof(uint) },
        { typeof(long), sizeof(long) },
        { typeof(ulong), sizeof(ulong) },
        { typeof(float), sizeof(float) },
        { typeof(double), sizeof(double) },
        { typeof(char), sizeof(char) },
        { typeof(IntPtr), sizeof(IntPtr) },
        { typeof(UIntPtr), sizeof(UIntPtr) }
    };

    private static readonly Dictionary<int, HookHandle> HookTab = new();
    private static readonly List<HookNode> HookNodes = [];
    public static readonly Harmony Harmony = new("tefkernel.HookManager");

    public static ushort HookMethod(MethodBase methodBase, IntPtr methodSignature, IntPtr prefixHook,
        IntPtr postfixHook)
    {
        var methodHash = methodBase.GetHashCode();
        var isNewHook = false;

        if (prefixHook == IntPtr.Zero && postfixHook == IntPtr.Zero)
            return 0;

        if (!HookTab.ContainsKey(methodHash))
        {
            // 创建新的Hook句柄
            var hookHandle = new HookHandle
            {
                Method = methodBase,
                MethodSignature = methodSignature,
                NodeIndexes = []
            };

            HookTab[methodHash] = hookHandle;
            isNewHook = true;
        }

        if (isNewHook)
            try
            {
                
                var harmonyMethodPrefix = prefixHook != IntPtr.Zero 
                    ? new HarmonyMethod(typeof(HookManager), nameof(PrefixHook)) 
                    : null;

                
                HarmonyMethod? harmonyMethodPostfix;
                if (methodBase is MethodInfo methodInfo)
                    harmonyMethodPostfix = postfixHook != IntPtr.Zero 
                        ? methodInfo.ReturnType == typeof(void)
                            ? new HarmonyMethod(typeof(HookManager), nameof(PostfixHookVoid))
                            : new HarmonyMethod(typeof(HookManager), nameof(PostfixHook))
                        : null;
                else
                    harmonyMethodPostfix = postfixHook != IntPtr.Zero 
                        ? new HarmonyMethod(typeof(HookManager), nameof(PostfixHookVoid))
                        : null;

                Harmony.Patch(methodBase, 
                    prefix: harmonyMethodPrefix,
                    postfix: harmonyMethodPostfix);
                

                Logger.Info($"Successfully hooked method: {methodBase.DeclaringType?.Name}.{methodBase.Name}");
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to apply Harmony patch for method {methodBase.Name}: {ex.Message}");
                HookTab.Remove(methodHash);
                return 0;
            }
        else
            Logger.Info($"Method {methodBase.Name} is already hooked, returning existing hook handle");

        return AddHookNode(methodHash, prefixHook, postfixHook);
    }

    public static bool UnhookMethodByNode(ushort nodeIndex)
    {
        if (nodeIndex >= HookNodes.Count)
        {
            Logger.Warning($"Invalid node index: {nodeIndex}");
            return false;
        }

        // 标记节点为删除状态
        HookNodes[nodeIndex] = new HookNode(IntPtr.Zero, IntPtr.Zero);

        // 查找对应的MethodHash
        var targetMethodHash = -1;

        foreach (var kvp in HookTab.Where(kvp => kvp.Value.NodeIndexes.Contains(nodeIndex)))
        {
            targetMethodHash = kvp.Key;
            break;
        }

        if (targetMethodHash == -1)
        {
            Logger.Warning($"No method found for node index: {nodeIndex}");
            return true; // 节点已标记为删除，返回成功
        }

        // 从Hook句柄中移除节点引用
        var hookHandle = HookTab[targetMethodHash];
        hookHandle.NodeIndexes.Remove(nodeIndex);

        // 如果这是该方法的最后一个Hook节点，则完全移除该方法
        if (hookHandle.NodeIndexes.Count == 0)
        {
            Harmony.Unpatch(hookHandle.Method, HarmonyPatchType.All, Harmony.Id);
            HookTab.Remove(targetMethodHash);
            Logger.Info(
                $"Unpatched and removed method: {hookHandle.Method.DeclaringType?.Name}.{hookHandle.Method.Name}");
        }
        else
        {
            HookTab[targetMethodHash] = hookHandle;
        }

        Logger.Info($"Marked hook node {nodeIndex} for deletion");
        return true;
    }

    private static ushort AddHookNode(int methodHash, IntPtr preHook, IntPtr postHook)
    {
        if (!HookTab.TryGetValue(methodHash, out var hookHandle))
        {
            Logger.Warning($"Method not hooked, hash: {methodHash}");
            return 0;
        }

        // 查找可用的空节点（标记为删除的节点）
        for (ushort i = 0; i < HookNodes.Count; i++)
            if (HookNodes[i].IsEmpty)
            {
                // 重用被标记为删除的节点
                HookNodes[i] = new HookNode(preHook, postHook);
                hookHandle.NodeIndexes.Add(i);
                HookTab[methodHash] = hookHandle;

                Logger.Info($"Reused deleted hook node {i} for method hash {methodHash}");
                return i;
            }

        // 没有可用的空节点，添加新节点
        var nodeIndex = (ushort)HookNodes.Count;
        var hookNode = new HookNode(preHook, postHook);
        HookNodes.Add(hookNode);

        // 更新Hook句柄
        hookHandle.NodeIndexes.Add(nodeIndex);
        HookTab[methodHash] = hookHandle;

        Logger.Info(
            $"Added hook function to method hash {methodHash}: pre={preHook}, post={postHook}, nodeIndex={nodeIndex}");
        return nodeIndex;
    }

    /// <summary>
    ///     检查方法是否只有一个Hook节点
    /// </summary>
    public static bool HasSingleHookNode(MethodBase methodBase)
    {
        var methodHash = methodBase.GetHashCode();

        if (!HookTab.TryGetValue(methodHash, out var hookHandle)) return false; // 方法未Hook

        return hookHandle.NodeIndexes.Count == 1;
    }

    public static IntPtr GetMethodSig(MethodBase methodBase)
    {
        var methodHash = methodBase.GetHashCode();

        return !HookTab.TryGetValue(methodHash, out var hookHandle) ? IntPtr.Zero : hookHandle.MethodSignature;
    }

    /// <summary>
    ///     检查方法是否已经被Hook
    /// </summary>
    public static bool IsMethodHooked(MethodBase methodBase)
    {
        var methodHash = methodBase.GetHashCode();
        return HookTab.ContainsKey(methodHash);
    }

    /// <summary>
    ///     检查Hook节点是否有效（未被标记为删除）
    /// </summary>
    public static bool IsHookNodeValid(ushort nodeIndex)
    {
        if (nodeIndex >= HookNodes.Count) return false;

        var node = HookNodes[nodeIndex];
        return !node.IsEmpty;
    }

    /// <summary>
    ///     通过Hook节点获取对应的方法
    /// </summary>
    /// <param name="nodeIndex">Hook节点ID</param>
    /// <returns>方法句柄，0表示未找到</returns>
    public static int GetMethodByNode(ushort nodeIndex)
    {
        if (nodeIndex >= HookNodes.Count)
        {
            Logger.Warning($"Invalid node index: {nodeIndex}");
            return 0;
        }

        var node = HookNodes[nodeIndex];
        if (node.IsEmpty)
        {
            Logger.Warning($"Hook node {nodeIndex} is empty");
            return 0;
        }

        // 查找包含此节点的方法
        foreach (var method in from kvp in HookTab
                 where kvp.Value.NodeIndexes.Contains(nodeIndex)
                 select kvp.Value.Method)
            return Asset.MethodInfos.Add(method);

        Logger.Warning($"No method found for hook node {nodeIndex}");
        return 0;
    }


    // Harmony前缀钩子
    private static bool PrefixHook(MethodBase __originalMethod, object? __instance, object[]? __args)
    {
        object? result = null;
        return ExecuteHook(__originalMethod, __instance, __args, ref result, true);
    }

    // Harmony后缀钩子
    private static void PostfixHookVoid(MethodBase __originalMethod, object? __instance, object[]? __args)
    {
        object? result = null;
        ExecuteHook(__originalMethod, __instance, __args, ref result, false);
    }

    private static void PostfixHook(MethodBase __originalMethod, object? __instance, object[]? __args,
        ref object? __result)
    {
        ExecuteHook(__originalMethod, __instance, __args, ref __result, false);
    }

    private static bool ExecuteHook(MethodBase originalMethod, object? instance, object[]? args, ref object? result,
        bool isPrefix)
    {
        try
        {
            var methodHash = originalMethod.GetHashCode();
            if (!HookTab.TryGetValue(methodHash, out var hookHandle))
                return true; // 继续执行原方法

            var methodInfo = originalMethod as MethodInfo;
            var isStatic = originalMethod.IsStatic;
            var parameters = originalMethod.GetParameters();

            // 处理实例
            var instanceIndex = -1;
            if (!isStatic && instance != null) instanceIndex = Asset.Objects.Add(instance);

            // 准备参数数组
            var argsPtr = (void**)Marshal.AllocHGlobal(IntPtr.Size * parameters.Length);
            var objectsToCleanup = new List<int>();
            var paramIndices = new int[parameters.Length];
            if (paramIndices == null) throw new ArgumentNullException(nameof(paramIndices));

            for (var i = 0; i < parameters.Length; i++)
            {
                var paramType = parameters[i].ParameterType;
                paramIndices[i] = -1;

                if (args?[i] != null)
                    paramIndices[i] = ProcessArgument(args[i], paramType, argsPtr, i, objectsToCleanup);
                else
                    argsPtr[i] = null;
            }

            // 处理返回值
            var resultPtr = IntPtr.Zero;
            var resultIndex = -1;
            var hasReturnValue = !isPrefix && methodInfo != null && methodInfo.ReturnType != typeof(void);

            if (hasReturnValue)
                // 创建返回值的副本
                resultIndex = ProcessArgument(result, methodInfo!.ReturnType, (void**)&resultPtr, 0, objectsToCleanup,
                    true);

            // 调用所有钩子节点
            const bool shouldContinue = true;
            foreach (var nodeIndex in hookHandle.NodeIndexes)
            {
                if (!IsHookNodeValid(nodeIndex))
                    continue;

                var node = HookNodes[nodeIndex];

                switch (isPrefix)
                {
                    case true when node.PrefixCallback != null:
                        try
                        {
                            node.PrefixCallback(instanceIndex, (IntPtr)argsPtr, hookHandle.MethodSignature);
                        }
                        catch (Exception ex)
                        {
                            Logger.Error($"Error in prefix hook node {nodeIndex}: {ex.Message}");
                        }

                        break;

                    case false when node.PostfixCallback != null:
                        try
                        {
                            // 传入当前的结果
                            node.PostfixCallback(instanceIndex, (IntPtr)argsPtr, resultPtr, hookHandle.MethodSignature);
                        }
                        catch (Exception ex)
                        {
                            Logger.Error($"Error in postfix hook node {nodeIndex}: {ex.Message}");
                        }

                        break;
                }
            }

            // 如果返回值被修改，从 resultPtr 中读取
            if (hasReturnValue && resultPtr != IntPtr.Zero)
            {
                var modifiedResult = RetrieveArgument(resultPtr, methodInfo!.ReturnType);
                if (modifiedResult != null) result = modifiedResult;
            }

            // 清理资源
            CleanupResources(argsPtr, parameters.Length, objectsToCleanup, resultIndex, resultPtr);

            // 清理实例
            if (instanceIndex != -1) Asset.Objects.RemoveAt(instanceIndex);

            return shouldContinue;
        }
        catch (Exception ex)
        {
            Logger.Error($"Error in {(isPrefix ? "Prefix" : "Postfix")}Hook: {ex.Message}");
            return true;
        }
    }

    private static int ProcessArgument(object? value, Type type, void** argsPtr, int index, List<int> objectsToCleanup,
        bool isReturnValue = false)
    {
        var objectIndex = -1;

        if (value == null)
        {
            // 处理 null 值
            if (isReturnValue)
                *(IntPtr*)argsPtr = IntPtr.Zero;
            else
                argsPtr[index] = null;
            return -1;
        }

        if (IsBasicType(type))
        {
            var size = GetTypeSize(type);
            var ptr = Marshal.AllocHGlobal(size);
            WriteBasicTypeValue(ptr, value, type);

            if (isReturnValue)
                *(IntPtr*)argsPtr = ptr;
            else
                argsPtr[index] = ptr.ToPointer();
        }
        else
        {
            objectIndex = Asset.Objects.Add(value);
            objectsToCleanup.Add(objectIndex);
            var ptr = Marshal.AllocHGlobal(sizeof(int));
            Marshal.WriteInt32(ptr, objectIndex);

            if (isReturnValue)
                *(IntPtr*)argsPtr = ptr;
            else
                argsPtr[index] = ptr.ToPointer();
        }

        return objectIndex;
    }

    private static object? RetrieveArgument(IntPtr ptr, Type type)
    {
        if (ptr == IntPtr.Zero)
            return GetDefaultValue(type);

        if (IsBasicType(type)) return ReadBasicTypeValue(ptr, type);

        var index = Marshal.ReadInt32(ptr);
        if (index >= 0 && index < Asset.Objects.Count) return Asset.Objects[index];

        return null;
    }

    private static void CleanupResources(void** argsPtr, int argsCount, List<int> objectsToCleanup, int resultIndex,
        IntPtr resultPtr)
    {
        // 清理参数内存
        for (var i = 0; i < argsCount; i++)
            if (argsPtr[i] != null)
                Marshal.FreeHGlobal(new IntPtr(argsPtr[i]));

        if (argsPtr != null) Marshal.FreeHGlobal(new IntPtr(argsPtr));

        // 清理返回值内存
        if (resultPtr != IntPtr.Zero) Marshal.FreeHGlobal(resultPtr);

        // 清理Objects中的对象引用
        foreach (var index in objectsToCleanup.Where(index => index != resultIndex)) Asset.Objects.RemoveAt(index);

        // 清理返回值
        if (resultIndex != -1) Asset.Objects.RemoveAt(resultIndex);
    }

    private static void WriteBasicTypeValue(IntPtr ptr, object value, Type type)
    {
        if (type == typeof(bool))
        {
            Marshal.WriteByte(ptr, (byte)((bool)value ? 1 : 0));
        }
        else if (type == typeof(byte))
        {
            Marshal.WriteByte(ptr, (byte)value);
        }
        else if (type == typeof(sbyte))
        {
            Marshal.WriteByte(ptr, (byte)(sbyte)value);
        }
        else if (type == typeof(short))
        {
            Marshal.WriteInt16(ptr, (short)value);
        }
        else if (type == typeof(ushort))
        {
            Marshal.WriteInt16(ptr, (short)(ushort)value);
        }
        else if (type == typeof(int))
        {
            Marshal.WriteInt32(ptr, (int)value);
        }
        else if (type == typeof(uint))
        {
            Marshal.WriteInt32(ptr, (int)(uint)value);
        }
        else if (type == typeof(long))
        {
            Marshal.WriteInt64(ptr, (long)value);
        }
        else if (type == typeof(ulong))
        {
            Marshal.WriteInt64(ptr, (long)(ulong)value);
        }
        else if (type == typeof(float))
        {
            var bytes = BitConverter.GetBytes((float)value);
            Marshal.Copy(bytes, 0, ptr, 4);
        }
        else if (type == typeof(double))
        {
            var bytes = BitConverter.GetBytes((double)value);
            Marshal.Copy(bytes, 0, ptr, 8);
        }
        else if (type == typeof(char))
        {
            Marshal.WriteInt16(ptr, (short)(char)value);
        }
        else if (type == typeof(IntPtr))
        {
            Marshal.WriteIntPtr(ptr, (IntPtr)value);
        }
        else if (type == typeof(UIntPtr))
        {
            Marshal.WriteIntPtr(ptr, (IntPtr)value);
        }
    }

    private static object? ReadBasicTypeValue(IntPtr ptr, Type type)
    {
        if (type == typeof(bool))
            return Marshal.ReadByte(ptr) != 0;
        if (type == typeof(byte))
            return Marshal.ReadByte(ptr);
        if (type == typeof(sbyte))
            return (sbyte)Marshal.ReadByte(ptr);
        if (type == typeof(short))
            return Marshal.ReadInt16(ptr);
        if (type == typeof(ushort))
            return (ushort)Marshal.ReadInt16(ptr);
        if (type == typeof(int))
            return Marshal.ReadInt32(ptr);
        if (type == typeof(uint))
            return (uint)Marshal.ReadInt32(ptr);
        if (type == typeof(long))
            return Marshal.ReadInt64(ptr);
        if (type == typeof(ulong))
            return (ulong)Marshal.ReadInt64(ptr);
        if (type == typeof(float))
        {
            var bytes = new byte[4];
            Marshal.Copy(ptr, bytes, 0, 4);
            return BitConverter.ToSingle(bytes, 0);
        }

        if (type == typeof(double))
        {
            var bytes = new byte[8];
            Marshal.Copy(ptr, bytes, 0, 8);
            return BitConverter.ToDouble(bytes, 0);
        }

        if (type == typeof(char))
            return (char)Marshal.ReadInt16(ptr);
        if (type == typeof(IntPtr) || type == typeof(UIntPtr))
            return Marshal.ReadIntPtr(ptr);

        return null;
    }

    private static object? GetDefaultValue(Type type)
    {
        return type.IsValueType ? Activator.CreateInstance(type) : null;
    }

    private static bool IsBasicType(Type type)
    {
        return type.IsPrimitive || type == typeof(IntPtr) || type == typeof(UIntPtr) ||
               type.IsEnum || type == typeof(decimal);
    }

    private static int GetTypeSize(Type type)
    {
        if (type.IsEnum)
            return Marshal.SizeOf(Enum.GetUnderlyingType(type));

        if (type == typeof(decimal))
            return sizeof(decimal);

        if (BasicTypeSizes.TryGetValue(type, out var size))
            return size;

        return IntPtr.Size;
    }

    // 定义C++委托类型
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void PrefixCallback(int instance, IntPtr args, IntPtr sigInfo);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void PostfixCallback(int instance, IntPtr args, IntPtr result, IntPtr sigInfo);

    private struct HookNode
    {
        public readonly PrefixCallback? PrefixCallback;
        public readonly PostfixCallback? PostfixCallback;
        public readonly IntPtr PreFix;
        public readonly IntPtr PostFix;
        public readonly bool IsEmpty;

        public HookNode(IntPtr preFix, IntPtr postFix)
        {
            PreFix = preFix;
            PostFix = postFix;

            // 缓存委托
            PrefixCallback = preFix != IntPtr.Zero
                ? Marshal.GetDelegateForFunctionPointer<PrefixCallback>(preFix)
                : null;
            PostfixCallback = postFix != IntPtr.Zero
                ? Marshal.GetDelegateForFunctionPointer<PostfixCallback>(postFix)
                : null;

            IsEmpty = PrefixCallback == null && PostfixCallback == null;
        }
    }

    private struct HookHandle
    {
        public MethodBase Method; // 保存原始方法引用
        public IntPtr MethodSignature; // 函数签名 
        public List<ushort> NodeIndexes; // Hook节点索引
    }
}