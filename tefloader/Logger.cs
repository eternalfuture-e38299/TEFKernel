// /*******************************************************************************
//  * tefkernel - Logger.cs
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

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace tefloader;

public static class Logger
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogTraceDelegate([MarshalAs(UnmanagedType.LPStr)] string message);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogDebugDelegate([MarshalAs(UnmanagedType.LPStr)] string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogInfoDelegate([MarshalAs(UnmanagedType.LPStr)] string message);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogWarningDelegate([MarshalAs(UnmanagedType.LPStr)] string message);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogErrorDelegate([MarshalAs(UnmanagedType.LPStr)] string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void LogCriticalDelegate([MarshalAs(UnmanagedType.LPStr)] string message);

    private static LogTraceDelegate? _traceRaw;
    private static LogDebugDelegate? _debugRaw;
    private static LogInfoDelegate? _infoRaw;
    private static LogWarningDelegate? _warningRaw;
    private static LogErrorDelegate? _errorRaw;
    private static LogCriticalDelegate? _criticalRaw;

    public static void Initialize(LibLoader handle)
    {
        try
        {
            _traceRaw = Marshal.GetDelegateForFunctionPointer<LogTraceDelegate>(
                handle.GetSym("NET_LOG_TRACE"));
                
            _debugRaw = Marshal.GetDelegateForFunctionPointer<LogDebugDelegate>(
                handle.GetSym("NET_LOG_DEBUG"));
                
            _infoRaw = Marshal.GetDelegateForFunctionPointer<LogInfoDelegate>(
                handle.GetSym("NET_LOG_INFO"));
                
            _warningRaw = Marshal.GetDelegateForFunctionPointer<LogWarningDelegate>(
                handle.GetSym("NET_LOG_WARNING"));
                
            _errorRaw = Marshal.GetDelegateForFunctionPointer<LogErrorDelegate>(
                handle.GetSym("NET_LOG_ERROR"));
                
            _criticalRaw = Marshal.GetDelegateForFunctionPointer<LogCriticalDelegate>(
                handle.GetSym("NET_LOG_CRITICAL"));

            Info("Logger system initialized successfully");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Logger.Initialize] Initialization failed: {ex.Message}");
        }
    }

   public static void Trace(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _traceRaw, filePath, lineNumber, memberName);

    public static void Debug(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _debugRaw, filePath, lineNumber, memberName);

    public static void Info(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _infoRaw, filePath, lineNumber, memberName);

    public static void Warning(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _warningRaw, filePath, lineNumber, memberName);

    public static void Error(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _errorRaw, filePath, lineNumber, memberName);

    public static void Critical(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, _criticalRaw, filePath, lineNumber, memberName);

    private static void Log(
        string message,
        Delegate? logDelegate,
        string filePath,
        int lineNumber,
        string memberName)
    {
        var formattedMessage = $"[{filePath}:{lineNumber} ({memberName})] {message}";
        
        switch (logDelegate)
        {
            case LogTraceDelegate trace: trace(formattedMessage); break;
            case LogDebugDelegate debug: debug(formattedMessage); break;
            case LogInfoDelegate info: info(formattedMessage); break;
            case LogWarningDelegate warning: warning(formattedMessage); break;
            case LogErrorDelegate error: error(formattedMessage); break;
            case LogCriticalDelegate critical: critical(formattedMessage); break;
        }
        
        if (logDelegate == null) Console.WriteLine(formattedMessage);
    }
}