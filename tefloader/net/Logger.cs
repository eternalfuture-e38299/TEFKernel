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
    private delegate void LogDelegate(int level, [MarshalAs(UnmanagedType.LPStr)] string file, int line, [MarshalAs(UnmanagedType.LPStr)] string func, [MarshalAs(UnmanagedType.LPStr)] string message);
    private static LogDelegate? _logRaw;

    public static void Initialize(LibLoader handle)
    {
        try
        {
            _logRaw = Marshal.GetDelegateForFunctionPointer<LogDelegate>(
                handle.GetSym("tefkernel_log_write_net"));
            
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
        => Log(message, 0, filePath, lineNumber, memberName);

    public static void Debug(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, 1, filePath, lineNumber, memberName);

    public static void Info(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, 2, filePath, lineNumber, memberName);

    public static void Warning(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, 3, filePath, lineNumber, memberName);

    public static void Error(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, 4, filePath, lineNumber, memberName);

    public static void Critical(
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0,
        [CallerMemberName] string memberName = "") 
        => Log(message, 5, filePath, lineNumber, memberName);

    private static void Log(
        string message,
        int level,
        string filePath,
        int lineNumber,
        string memberName)
    {
        if (_logRaw == null) Console.WriteLine($"[{filePath}:{lineNumber} ({memberName})] {message}");
        else _logRaw(level, filePath, lineNumber, memberName, message);
    }
}