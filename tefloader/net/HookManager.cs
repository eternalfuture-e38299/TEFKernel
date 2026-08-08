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

namespace tefloader;

public static unsafe class HookManager
{
    private const short PATCH_HOOK_INVALID_ID = 0;

    // 缓存
    private static readonly Dictionary<int, HookHandle> HookTab = new();
    private static readonly List<HookNode> HookNodes = [];

    public static readonly Harmony Harmony = new("tefkernel.HookManager");

    /// <summary>
    ///     安装前缀和后缀 Hook
    /// </summary>
    public static short il2cpp_hook_method(IntPtr methodPtr, IntPtr methodSignature, IntPtr prefixHook,
        IntPtr postfixHook)
    {
        if (prefixHook == IntPtr.Zero && postfixHook == IntPtr.Zero)
        {
            Logger.Error("Cannot install hook: both prefix and postfix are NULL");
            return PATCH_HOOK_INVALID_ID;
        }

        if (methodPtr == IntPtr.Zero)
        {
            Logger.Error("Cannot install hook: methodPtr is NULL");
            return PATCH_HOOK_INVALID_ID;
        }

        MethodBase? methodBase;
        try
        {
            var gcHandle = GCHandle.FromIntPtr(methodPtr);
            if (!gcHandle.IsAllocated)
            {
                // ReSharper disable once InterpolatedStringExpressionIsNotIFormattable
                Logger.Error($"GCHandle is not allocated for pointer: 0x{methodPtr:X16}");
                return PATCH_HOOK_INVALID_ID;
            }

            methodBase = gcHandle.Target as MethodBase;
            if (methodBase == null)
            {
                Logger.Error($"GCHandle target is not a MethodBase: {gcHandle.Target?.GetType().FullName ?? "null"}");
                return PATCH_HOOK_INVALID_ID;
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get MethodBase from GCHandle: {ex.Message}");
            return PATCH_HOOK_INVALID_ID;
        }

        var methodToken = methodBase.MetadataToken;
        var isNewHook = false;

        if (!HookTab.ContainsKey(methodToken))
        {
            var hookHandle = new HookHandle
            {
                Method = methodBase,
                MethodSignature = methodSignature,
                NodeIndexes = []
            };

            HookTab[methodToken] = hookHandle;
            isNewHook = true;
        }

        if (!isNewHook) return AddHookNode(methodToken, prefixHook, postfixHook);
        {
            try
            {
                var isVoid = methodBase is MethodInfo mi && mi.ReturnType == typeof(void);

                var harmonyPrefix = prefixHook != IntPtr.Zero
                    ? isVoid
                        ? new HarmonyMethod(typeof(HookManager), nameof(UniversalPrefixVoid))
                        : new HarmonyMethod(typeof(HookManager), nameof(UniversalPrefixNonVoid))
                    : null;

                var harmonyPostfix = postfixHook != IntPtr.Zero
                    ? isVoid
                        ? new HarmonyMethod(typeof(HookManager), nameof(UniversalPostfixVoid))
                        : new HarmonyMethod(typeof(HookManager), nameof(UniversalPostfixNonVoid))
                    : null;

                Harmony.Patch(methodBase, harmonyPrefix, harmonyPostfix);

                Logger.Info(
                    $"Successfully hooked method: {methodBase.DeclaringType?.Name}.{methodBase.Name} (Token: {methodToken})");
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to apply Harmony patch for {methodBase.Name}: {ex.Message}");
                HookTab.Remove(methodToken);
                return PATCH_HOOK_INVALID_ID;
            }
        }

        return AddHookNode(methodToken, prefixHook, postfixHook);
    }

    /// <summary>
    ///     卸载 Hook
    /// </summary>
    public static bool il2cpp_unhook_method(short nodeIndex)
    {
        if (nodeIndex >= HookNodes.Count)
        {
            Logger.Warning($"Invalid node index: {nodeIndex}");
            return false;
        }

        HookNodes[nodeIndex] = HookNode.Empty;

        var targetMethodToken = -1;
        foreach (var kvp in HookTab.Where(kvp => kvp.Value.NodeIndexes.Contains(nodeIndex)))
        {
            targetMethodToken = kvp.Key;
            break;
        }

        if (targetMethodToken == -1)
        {
            Logger.Warning($"No method found for node index: {nodeIndex}");
            return true;
        }

        var hookHandle = HookTab[targetMethodToken];
        hookHandle.NodeIndexes.Remove(nodeIndex);

        if (hookHandle.NodeIndexes.Count == 0)
        {
            Harmony.Unpatch(hookHandle.Method, HarmonyPatchType.All, Harmony.Id);
            HookTab.Remove(targetMethodToken);
            Logger.Info(
                $"Unpatched and removed method: {hookHandle.Method.DeclaringType?.Name}.{hookHandle.Method.Name}");
        }

        return true;
    }

    /// <summary>
    ///     添加 Hook 节点
    /// </summary>
    private static short AddHookNode(int methodToken, IntPtr preHook, IntPtr postHook)
    {
        if (!HookTab.TryGetValue(methodToken, out var hookHandle))
        {
            Logger.Warning($"Method not hooked, token: {methodToken}");
            return PATCH_HOOK_INVALID_ID;
        }

        for (short i = 0; i < HookNodes.Count; i++)
            if (HookNodes[i].IsEmpty)
            {
                HookNodes[i] = new HookNode(preHook, postHook);
                hookHandle.NodeIndexes.Add(i);
                return i;
            }

        var nodeIndex = (short)HookNodes.Count;
        HookNodes.Add(new HookNode(preHook, postHook));
        hookHandle.NodeIndexes.Add(nodeIndex);

        return nodeIndex;
    }

    #region 核心抽象方法

    /// <summary>
    ///     准备 Hook 执行环境
    /// </summary>
    private static HookContext PrepareContext(MethodBase method, object? instance, object[]? args, ref object? result)
    {
        var context = new HookContext
        {
            MethodToken = method.MetadataToken,
            MethodInfo = method as MethodInfo,
            InstanceHandle = IntPtr.Zero,
            ArgsPtr = null,
            ResultPtr = IntPtr.Zero,
            AllocatedPointers = [],
            GcHandlesToFree = []
        };

        if (context.MethodInfo == null)
            return context;

        // 处理实例
        if (!method.IsStatic && instance != null)
            context.InstanceHandle = Utils.ObjectToPtr(instance);

        // 准备参数指针数组
        var parameters = method.GetParameters();
        context.ArgsPtr = (void**)Marshal.AllocHGlobal(IntPtr.Size * parameters.Length);
        context.OriginalArgs = new object?[parameters.Length];

        for (var i = 0; i < parameters.Length; i++)
        {
            context.OriginalArgs[i] = args?.Length > i ? args[i] : null;
            
            // ★ 特殊处理 ref 和 out 参数
            var paramInfo = parameters[i];
            var isRefOrOut = paramInfo.ParameterType.IsByRef || paramInfo.IsOut;
            
            if (isRefOrOut)
            {
                // ref/out 参数：分配指针大小的内存，指向实际数据
                var elementType = paramInfo.ParameterType.GetElementType()!;
                var ptr = Marshal.AllocHGlobal(IntPtr.Size);
                context.AllocatedPointers.Add(ptr);
                
                if (context.OriginalArgs[i] != null)
                {
                    // 分配实际数据内存并写入值
                    var dataPtr = Marshal.AllocHGlobal(Utils.GetTypeSize(elementType));
                    context.AllocatedPointers.Add(dataPtr);
                    Utils.SetNativeValue(dataPtr, context.OriginalArgs[i]);
                    Marshal.WriteIntPtr(ptr, dataPtr);
                }
                else
                {
                    Marshal.WriteIntPtr(ptr, IntPtr.Zero);
                }
                
                context.ArgsPtr[i] = ptr.ToPointer();
                Logger.Debug($"PrepareContext: ref/out param {i}, type={elementType.Name}, ptr={ptr}");
            }
            else
            {
                // 普通参数
                var argPtr = MarshalArgument(context.OriginalArgs[i], parameters[i].ParameterType, 
                    context.AllocatedPointers, context.GcHandlesToFree);
                context.ArgsPtr[i] = argPtr.ToPointer();
            }
        }

        // 准备返回值指针
        var returnType = context.MethodInfo.ReturnType;
        if (returnType == typeof(void)) return context;
        context.ResultPtr = Marshal.AllocHGlobal(Utils.GetTypeSize(returnType));
        context.AllocatedPointers.Add(context.ResultPtr);
        if (result != null)
            Utils.SetNativeValue(context.ResultPtr, result);

        return context;
    }

    /// <summary>
    ///     执行 Prefix Hooks
    /// </summary>
    private static bool ExecutePrefixHooks(HookHandle hookHandle, HookContext context)
    {
        var shouldSkip = false;
        foreach (var nodeIndex in hookHandle.NodeIndexes)
        {
            if (!IsHookNodeValid(nodeIndex))
                continue;

            var node = HookNodes[nodeIndex];
            if (node.PrefixCallback == null)
                continue;

            try
            {
                if (node.PrefixCallback(context.InstanceHandle, (IntPtr)context.ArgsPtr, 
                    hookHandle.MethodSignature, context.ResultPtr))
                    shouldSkip = true;
            }
            catch (Exception ex)
            {
                Logger.Error($"Error in prefix hook node {nodeIndex}: {ex.Message}");
            }
        }
        return shouldSkip;
    }

    /// <summary>
    ///     执行 Postfix Hooks
    /// </summary>
    private static void ExecutePostfixHooks(HookHandle hookHandle, HookContext context)
    {
        foreach (var nodeIndex in hookHandle.NodeIndexes)
        {
            if (!IsHookNodeValid(nodeIndex))
                continue;

            var node = HookNodes[nodeIndex];
            if (node.PostfixCallback == null)
                continue;

            try
            {
                node.PostfixCallback(context.InstanceHandle, (IntPtr)context.ArgsPtr, 
                    context.ResultPtr, hookHandle.MethodSignature);
            }
            catch (Exception ex)
            {
                Logger.Error($"Error in postfix hook node {nodeIndex}: {ex.Message}");
            }
        }
    }

    /// <summary>
    ///     将修改后的参数写回 Harmony 参数数组
    /// </summary>
    private static void WriteBackParameters(MethodBase method, object[]? args, HookContext context)
    {
        if (args == null) return;
    
        var parameters = method.GetParameters();
        for (var i = 0; i < parameters.Length && i < args.Length; i++)
        {
            var paramInfo = parameters[i];
            var isRefOrOut = paramInfo.ParameterType.IsByRef || paramInfo.IsOut;
            var argPtr = (IntPtr)context.ArgsPtr[i];
        
            if (argPtr == IntPtr.Zero) continue;
        
            if (isRefOrOut)
            {
                // ★ ref/out 参数：读取两次指针
                // 第一次：读取指向实际数据的指针
                var dataPtrPtr = Marshal.ReadIntPtr(argPtr);
                if (dataPtrPtr == IntPtr.Zero) continue;
                
                var elementType = paramInfo.ParameterType.GetElementType()!;
                
                // 第二次：从实际数据指针读取值
                if (Utils.IsValueType(elementType) || elementType.IsEnum)
                {
                    var newValue = Utils.GetNativeValue(dataPtrPtr, elementType);
                    if (newValue != null)
                        args[i] = newValue;
                }
                else
                {
                    // 引用类型：从 GCHandle 读取
                    var handlePtr = Marshal.ReadIntPtr(dataPtrPtr);
                    if (handlePtr != IntPtr.Zero)
                    {
                        var handle = GCHandle.FromIntPtr(handlePtr);
                        if (handle.IsAllocated)
                            args[i] = handle.Target;
                    }
                }
                
                Logger.Debug($"WriteBackParameters: ref/out param {i}, value={args[i]}");
            }
            else if (!Utils.IsValueType(paramInfo.ParameterType))
            {
                // 普通引用类型参数
                var handlePtr = Marshal.ReadIntPtr(argPtr);
                if (handlePtr == IntPtr.Zero) continue;
                var handle = GCHandle.FromIntPtr(handlePtr);
                if (handle.IsAllocated && handle.Target != args[i])
                    args[i] = handle.Target;
            }
        }
    }

    /// <summary>
    ///     读取修改后的返回值
    /// </summary>
    private static object? ReadBackResult(MethodInfo methodInfo, HookContext context, object? currentResult)
    {
        var returnType = methodInfo.ReturnType;
        if (returnType == typeof(void) || context.ResultPtr == IntPtr.Zero)
            return currentResult;

        var finalResult = Utils.GetNativeValue(context.ResultPtr, returnType);
        return finalResult ?? currentResult;
    }

    /// <summary>
    ///     清理 Hook 上下文资源
    /// </summary>
    private static void CleanupContext(HookContext context)
    {
        foreach (var ptr in context.AllocatedPointers.Where(ptr => ptr != IntPtr.Zero))
            Marshal.FreeHGlobal(ptr);
        
        foreach (var handle in context.GcHandlesToFree.Where(handle => handle.IsAllocated))
            handle.Free();
        
        if (context.InstanceHandle != IntPtr.Zero)
            GCHandle.FromIntPtr(context.InstanceHandle).Free();
        
        if (context.ArgsPtr != null)
            Marshal.FreeHGlobal((IntPtr)context.ArgsPtr);
    }

    #endregion

    #region Harmony Patch 方法

    /// <summary>
    ///     通用 Prefix Hook (Void 返回值)
    /// </summary>
    [HarmonyPrefix]
    public static bool UniversalPrefixVoid(MethodBase __originalMethod, object? __instance, object[]? __args)
    {
        try
        {
            var methodToken = __originalMethod.MetadataToken;
            if (!HookTab.TryGetValue(methodToken, out var hookHandle))
                return true;

            object? dummyResult = null;
            var context = PrepareContext(__originalMethod, __instance, __args, ref dummyResult);
            
            if (context.MethodInfo == null)
            {
                CleanupContext(context);
                return true;
            }

            var shouldSkip = ExecutePrefixHooks(hookHandle, context);
            
            // 对于 void 方法，只需要写回 ref/out 参数
            WriteBackParameters(__originalMethod, __args, context);
            
            CleanupContext(context);
            return !shouldSkip;
        }
        catch (Exception ex)
        {
            Logger.Error($"Error in UniversalPrefixVoid: {ex.Message}");
            return true;
        }
    }

    /// <summary>
    ///     通用 Prefix Hook (非 Void 返回值)
    /// </summary>
    [HarmonyPrefix]
    public static bool UniversalPrefixNonVoid(MethodBase __originalMethod, object? __instance, object[]? __args,
        ref object? __result)
    {
        try
        {
            var methodToken = __originalMethod.MetadataToken;
            if (!HookTab.TryGetValue(methodToken, out var hookHandle))
                return true;

            var context = PrepareContext(__originalMethod, __instance, __args, ref __result);
            
            if (context.MethodInfo == null)
            {
                CleanupContext(context);
                return true;
            }

            var shouldSkip = ExecutePrefixHooks(hookHandle, context);
            
            // 写回修改的参数
            WriteBackParameters(__originalMethod, __args, context);
            
            // 读取修改后的返回值
            __result = ReadBackResult(context.MethodInfo, context, __result);
            
            CleanupContext(context);
            return !shouldSkip;
        }
        catch (Exception ex)
        {
            Logger.Error($"Error in UniversalPrefixNonVoid: {ex.Message}");
            return true;
        }
    }

    /// <summary>
    ///     通用 Postfix Hook (Void 返回值)
    /// </summary>
    [HarmonyPostfix]
    public static void UniversalPostfixVoid(MethodBase __originalMethod, object? __instance, object[]? __args)
    {
        try
        {
            var methodToken = __originalMethod.MetadataToken;
            if (!HookTab.TryGetValue(methodToken, out var hookHandle))
                return;

            object? dummyResult = null;
            var context = PrepareContext(__originalMethod, __instance, __args, ref dummyResult);
            
            if (context.MethodInfo == null)
            {
                CleanupContext(context);
                return;
            }

            ExecutePostfixHooks(hookHandle, context);
            
            // void 方法只需要写回 ref/out 参数
            WriteBackParameters(__originalMethod, __args, context);
            
            CleanupContext(context);
        }
        catch (Exception ex)
        {
            Logger.Error($"Error in UniversalPostfixVoid: {ex.Message}");
        }
    }

    /// <summary>
    ///     通用 Postfix Hook (非 Void 返回值)
    /// </summary>
    [HarmonyPostfix]
    public static void UniversalPostfixNonVoid(MethodBase __originalMethod, object? __instance, object[]? __args,
        ref object? __result)
    {
        try
        {
            var methodToken = __originalMethod.MetadataToken;
            if (!HookTab.TryGetValue(methodToken, out var hookHandle))
                return;

            var context = PrepareContext(__originalMethod, __instance, __args, ref __result);
            
            if (context.MethodInfo == null)
            {
                CleanupContext(context);
                return;
            }

            ExecutePostfixHooks(hookHandle, context);
            
            // 写回修改的参数
            WriteBackParameters(__originalMethod, __args, context);
            
            // 读取修改后的返回值
            __result = ReadBackResult(context.MethodInfo, context, __result);
            
            CleanupContext(context);
        }
        catch (Exception ex)
        {
            Logger.Error($"Error in UniversalPostfixNonVoid: {ex.Message}");
        }
    }

    #endregion

    private static IntPtr MarshalArgument(object? value, Type type, List<IntPtr> allocatedPointers,
        List<GCHandle> gcHandlesToFree)
    {
        if (value == null)
            return IntPtr.Zero;

        // 检查是否为基本类型（对应 C 端的 patch_type_t）
        if (Utils.IsValueType(type))
        {
            // 基本类型：直接分配内存并写入值
            var ptr = Marshal.AllocHGlobal(Utils.GetTypeSize(type));
            allocatedPointers.Add(ptr);
            Utils.SetNativeValue(ptr, value);
            return ptr;
        }

        // 所有非基本类型都作为 PATCH_OBJECT 处理
        // 只需要分配一个指针大小的内存来存储对象的 GCHandle
        var objPtr = Marshal.AllocHGlobal(IntPtr.Size);
        allocatedPointers.Add(objPtr);
    
        var handle = GCHandle.Alloc(value, GCHandleType.Normal);
        gcHandlesToFree.Add(handle);
        Marshal.WriteIntPtr(objPtr, GCHandle.ToIntPtr(handle));
    
        return objPtr;
    }
    
    public static bool il2cpp_has_single_hook_node(IntPtr methodPtr)
    {
        var methodBase = (MethodBase)GCHandle.FromIntPtr(methodPtr).Target;
        var methodToken = methodBase.MetadataToken;
        return HookTab.TryGetValue(methodToken, out var hookHandle) && hookHandle.NodeIndexes.Count == 1;
    }

    public static IntPtr il2cpp_get_hooked_method_sig(IntPtr methodPtr)
    {
        var methodBase = (MethodBase)GCHandle.FromIntPtr(methodPtr).Target;
        var methodToken = methodBase.MetadataToken;
        return HookTab.TryGetValue(methodToken, out var hookHandle) ? hookHandle.MethodSignature : IntPtr.Zero;
    }

    public static bool il2cpp_is_method_hooked(IntPtr methodPtr)
    {
        var methodBase = (MethodBase)GCHandle.FromIntPtr(methodPtr).Target;
        var methodToken = methodBase.MetadataToken;
        return HookTab.ContainsKey(methodToken);
    }

    private static bool IsHookNodeValid(short nodeIndex)
    {
        return nodeIndex < HookNodes.Count && !HookNodes[nodeIndex].IsEmpty;
    }

    public static IntPtr il2cpp_get_method_by_hook_node(short nodeIndex)
    {
        if (nodeIndex < 0 || nodeIndex >= HookNodes.Count)
        {
            Logger.Warning($"Invalid node index: {nodeIndex}");
            return IntPtr.Zero;
        }

        if (HookNodes[nodeIndex].IsEmpty)
        {
            Logger.Warning($"Hook node {nodeIndex} is empty");
            return IntPtr.Zero;
        }

        foreach (var kvp in HookTab.Where(kvp => kvp.Value.NodeIndexes.Contains(nodeIndex)))
            return Utils.ObjectToPtr(kvp.Value.Method);

        return IntPtr.Zero;
    }

    #region 辅助结构

    /// <summary>
    ///     Hook 执行上下文
    /// </summary>
    private class HookContext
    {
        public int MethodToken;
        public MethodInfo? MethodInfo;
        public IntPtr InstanceHandle;
        public void** ArgsPtr;
        public IntPtr ResultPtr;
        public List<IntPtr> AllocatedPointers = [];
        public List<GCHandle> GcHandlesToFree = [];
        public object?[]? OriginalArgs;
    }

    // 回调委托定义
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate bool PrefixCallback(IntPtr instance, IntPtr args, IntPtr sigInfo, IntPtr result);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void PostfixCallback(IntPtr instance, IntPtr args, IntPtr result, IntPtr sigInfo);

    private struct HookNode
    {
        public readonly PrefixCallback? PrefixCallback;
        public readonly PostfixCallback? PostfixCallback;
        public readonly IntPtr PreFix;
        public readonly IntPtr PostFix;
        public readonly bool IsEmpty;

        public static readonly HookNode Empty = new(true);

        private HookNode(bool isEmpty)
        {
            PreFix = IntPtr.Zero;
            PostFix = IntPtr.Zero;
            PrefixCallback = null;
            PostfixCallback = null;
            IsEmpty = isEmpty;
        }

        public HookNode(IntPtr preFix, IntPtr postFix)
        {
            PreFix = preFix;
            PostFix = postFix;

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
        public MethodBase Method;
        public IntPtr MethodSignature;
        public List<short> NodeIndexes;
    }

    #endregion
}