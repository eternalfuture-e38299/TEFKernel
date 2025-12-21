// /*******************************************************************************
//  * tefkernel - LibLoader.cs
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

using System.Runtime.InteropServices;

namespace tefloader;

public class LibLoader
{
    // Unix加载标志
    [Flags]
    public enum UnixFlags
    {
        Lazy = 0x001,
        Now = 0x002,
        Global = 0x100,
        Local = 0x000,
        NoDelete = 0x010,
        NoLoad = 0x004,
        DeepBind = 0x008
    }

    // Windows加载标志
    [Flags]
    public enum WinFlags
    {
        DontResolve = 0x0001,
        LoadAsData = 0x0002,
        AlterSearch = 0x0008,
        IgnoreAuth = 0x0010,
        LoadAsImage = 0x0020,
        Exclusive = 0x0040,
        SearchDllDir = 0x0100,
        SearchAppDir = 0x0200,
        SearchUser = 0x0400,
        SearchSys32 = 0x0800,
        SearchDefault = 0x1000
    }

    // 平台检测
    private static bool IsWin => Environment.OSVersion.Platform == PlatformID.Win32NT;

    private static bool IsLinux
    {
        get
        {
            if (Environment.OSVersion.Platform == PlatformID.Win32NT)
                return false;
        
            try
            {
                return File.Exists("/etc/os-release") || 
                       File.Exists("/etc/lsb-release") ||
                       Directory.Exists("/proc") && File.Exists("/proc/version");
            }
            catch
            {
                return (int)Environment.OSVersion.Platform == 4;
            }
        }
    }

    private static bool IsMac
    {
        get
        {
            if (Environment.OSVersion.Platform == PlatformID.MacOSX)
                return true;
        
            if ((int)Environment.OSVersion.Platform == 6)
                return true;
        
            return Directory.Exists("/Applications") && 
                   Directory.Exists("/System/Library") &&
                   !File.Exists("/etc/os-release"); // 确保不是Linux
        }
    }
    
    private static string GetExt()
    {
        return IsWin ? ".dll" : IsLinux ? ".so" : IsMac ? ".dylib" : "";
    }

    public LibLoader() { }

    public LibLoader(string path)
    {
        LoadLib(path);
    }

    public void LoadLib(string path, bool addExt = false)
    {
        if (IsWin)
            LoadLib(path, WinFlags.AlterSearch, addExt);
        else
            LoadLib(path, UnixFlags.Now, addExt);
    }

    public void LoadLib(string path, UnixFlags mode, bool addExt = false)
    {
        if (IsWin) throw new PlatformNotSupportedException("The Unix mode cannot be used on Windows.");
        LoadInternal(path, (int)mode, addExt);
    }

    public void LoadLib(string path, WinFlags mode, bool addExt = false)
    {
        if (!IsWin) throw new PlatformNotSupportedException("Windows mode cannot be used on Unix");
        LoadInternal(path, (int)mode, addExt);
    }

    public IntPtr GetSym(string sym)
    {
        return IsWin ? GetProcAddress(_handle, sym) :
            IsLinux ? linux_dlsym(_handle, sym) :
            mac_dlsym(_handle, sym);
    }

    public T GetVariable<T>(string variableName)
    {
        var address = GetSym(variableName);
        return Marshal.PtrToStructure<T>(address);
    }

    public void SetVariable<T>(string variableName, T value)
    {
        var address = GetSym(variableName);
        Marshal.StructureToPtr(value, address, false);
    }

    public bool UnLoad()
    {
        return IsWin ? FreeLibrary(_handle) :
        IsLinux ? linux_dlclose(_handle) == 1 :
        mac_dlclose(_handle) == 1;
    }

    private void LoadInternal(string path, int mode, bool addExt)
    {
        try
        {
            if (addExt) path += GetExt();
            path = Path.GetFullPath(path);

            _handle = IsWin ? LoadLibraryExW(path, IntPtr.Zero, (uint)mode) :
                IsLinux ? linux_dlopen(path, mode) :
                mac_dlopen(path, mode);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Load error: {ex}");
            throw;
        }
    }
    
    private IntPtr _handle = IntPtr.Zero;
    
    // Windows
    [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool FreeLibrary(IntPtr hModule);
    
    [DllImport("kernel32", CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryExW(string lpFileName, IntPtr hFile, uint dwFlags);
    
    // Linux
    [DllImport("libdl.so", EntryPoint = "dlopen")]
    private static extern IntPtr linux_dlopen(string filename, int flags);

    [DllImport("libdl.so", EntryPoint = "dlsym")]
    private static extern IntPtr linux_dlsym(IntPtr handle, string symbol);

    [DllImport("libdl.so", EntryPoint = "dlclose")]
    private static extern int linux_dlclose(IntPtr handle);

    // MacOs
    [DllImport("libdl.dylib", EntryPoint = "dlopen")]
    private static extern IntPtr mac_dlopen(string filename, int flags);

    [DllImport("libdl.dylib", EntryPoint = "dlsym")]
    private static extern IntPtr mac_dlsym(IntPtr handle, string symbol);

    [DllImport("libdl.dylib", EntryPoint = "dlclose")]
    private static extern int mac_dlclose(IntPtr handle);
}