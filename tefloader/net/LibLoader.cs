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

    // 检测是否在 Wine 环境下运行
    private static bool IsRunningInWine
    {
        get
        {
            try
            {
                [DllImport("ntdll")]
                static extern IntPtr wine_get_version();
                
                var version = wine_get_version();
                if (version != IntPtr.Zero)
                {
                    return true;
                }
            }
            catch
            {
                // ignored
            }

            try
            {
                // 方法2：检查环境变量
                var wineEnv = Environment.GetEnvironmentVariable("WINEPREFIX");
                if (!string.IsNullOrEmpty(wineEnv))
                    return true;
                    
                // 方法3：检查特定注册表项
                using var key = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"Software\Wine");
                if (key != null)
                    return true;
            }
            catch
            {
                // ignored
            }

            return false;
        }
    }
    
    // 检测是否为真实的 Windows（不是 Wine）
    private static bool IsRealWindows
    {
        get
        {
            if (Environment.OSVersion.Platform != PlatformID.Win32NT)
                return false;
            
            // 如果在 Wine 中，返回 false
            if (IsRunningInWine)
                return false;
            
            return true;
        }
    }

    private static bool IsLinux
    {
        get
        {
            // 如果在 Wine 中，不使用 Linux API
            if (IsRunningInWine)
                return false;
                
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
            if (IsRunningInWine)
                return false;
                
            if (Environment.OSVersion.Platform == PlatformID.MacOSX)
                return true;
        
            if ((int)Environment.OSVersion.Platform == 6)
                return true;
        
            return Directory.Exists("/Applications") && 
                   Directory.Exists("/System/Library") &&
                   !File.Exists("/etc/os-release");
        }
    }
    
    // 判断应该使用哪种 API
    private static bool ShouldUseWindowsApi => IsRealWindows || IsRunningInWine;
    
    private static string GetExt()
    {
        if (ShouldUseWindowsApi) return ".dll";
        if (IsLinux) return ".so";
        if (IsMac) return ".dylib";
        return "";
    }

    public LibLoader() { }

    public LibLoader(string path)
    {
        LoadLib(path);
    }

    public void LoadLib(string path, bool addExt = false)
    {
        if (ShouldUseWindowsApi)
            LoadLib(path, WinFlags.LoadAsData | WinFlags.SearchDllDir, addExt);
        else
            LoadLib(path, UnixFlags.Now, addExt);
    }

    public void LoadLib(string path, UnixFlags mode, bool addExt = false)
    {
        if (ShouldUseWindowsApi) throw new PlatformNotSupportedException("The Unix mode cannot be used on Windows/Wine.");
        LoadInternal(path, (int)mode, addExt);
    }

    public void LoadLib(string path, WinFlags mode, bool addExt = false)
    {
        if (!ShouldUseWindowsApi) throw new PlatformNotSupportedException("Windows mode cannot be used on Unix");
        LoadInternal(path, (int)mode, addExt);
    }

    public IntPtr GetSym(string sym)
    {
        // 在 Wine 或真实 Windows 上，只使用 Windows API
        if (!ShouldUseWindowsApi) return IsLinux ? linux_dlsym(_handle, sym) : mac_dlsym(_handle, sym);
        var result = GetProcAddress(_handle, sym);

        if (result != IntPtr.Zero || !IsRunningInWine) return result;
        var error = Marshal.GetLastWin32Error();
        Console.Error.WriteLine($"GetProcAddress failed for '{sym}', error: {error}");

        return result;

    }
    
    // 通过序号获取符号（备用方案）
    public IntPtr GetSymByOrdinal(int ordinal)
    {
        if (ShouldUseWindowsApi)
        {
            return GetProcAddressByOrdinal(_handle, ordinal);
        }
        return IntPtr.Zero;
    }

    public T? GetVariable<T>(string variableName)
    {
        var address = GetSym(variableName);
        return address == IntPtr.Zero ? default : Marshal.PtrToStructure<T>(address);
    }

    public void SetVariable<T>(string variableName, T value)
    {
        var address = GetSym(variableName);
        if (address == IntPtr.Zero)
            return;
        Marshal.StructureToPtr(value, address, false);
    }

    public bool UnLoad()
    {
        if (ShouldUseWindowsApi)
            return FreeLibrary(_handle);
        if (IsLinux)
            return linux_dlclose(_handle) == 1;
        return mac_dlclose(_handle) == 1;
    }
    
    public IntPtr GetHandle()
    {
        return _handle;
    }

    private void LoadInternal(string path, int mode, bool addExt)
    {
        try
        {
            if (addExt) path += GetExt();
            path = Path.GetFullPath(path);

            if (ShouldUseWindowsApi)
            {
                // 在 Wine 下，使用 LoadLibrary 而不是 LoadLibraryExW
                if (IsRunningInWine)
                {
                    _handle = LoadLibraryW(path);
                    if (_handle == IntPtr.Zero)
                    {
                        int error = Marshal.GetLastWin32Error();
                        throw new Exception($"LoadLibraryW failed with error: {error}");
                    }
                }
                else
                {
                    _handle = LoadLibraryExW(path, IntPtr.Zero, (uint)mode);
                    if (_handle != IntPtr.Zero) return;
                    Marshal.GetLastWin32Error();
                    _handle = LoadLibraryW(path);

                    if (_handle != IntPtr.Zero) return;
                    var error = Marshal.GetLastWin32Error();
                    throw new Exception($"Failed to load library with both methods. Last error: {error}");
                }
            }
            else if (IsLinux)
            {
                _handle = linux_dlopen(path, mode);
                if (_handle != IntPtr.Zero) return;
                var errorPtr = linux_dlerror();
                var error = Marshal.PtrToStringAnsi(errorPtr);
                throw new Exception($"dlopen failed: {error}");
            }
            else
            {
                _handle = mac_dlopen(path, mode);
                if (_handle != IntPtr.Zero) return;
                var errorPtr = mac_dlerror();
                var error = Marshal.PtrToStringAnsi(errorPtr);
                throw new Exception($"dlopen failed: {error}");
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Load error: {ex}");
            throw;
        }
    }
    
    private IntPtr _handle = IntPtr.Zero;
    
    // Windows APIs
    [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);
    
    [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddressByOrdinal(IntPtr hModule, int ordinal);

    [DllImport("kernel32")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool FreeLibrary(IntPtr hModule);
    
    [DllImport("kernel32", CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryExW(string lpFileName, IntPtr hFile, uint dwFlags);
    
    [DllImport("kernel32", CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryW(string lpFileName);
    
    // Linux/POSIX APIs - 只在非 Wine 环境下使用
    [DllImport("libc", EntryPoint = "dlopen")]
    private static extern IntPtr linux_dlopen(string filename, int flags);

    [DllImport("libc", EntryPoint = "dlsym")]
    private static extern IntPtr linux_dlsym(IntPtr handle, string symbol);

    [DllImport("libc", EntryPoint = "dlclose")]
    private static extern int linux_dlclose(IntPtr handle);
    
    [DllImport("libc", EntryPoint = "dlerror")]
    private static extern IntPtr linux_dlerror();
    
    // macOS APIs
    [DllImport("libdl.dylib", EntryPoint = "dlopen")]
    private static extern IntPtr mac_dlopen(string filename, int flags);

    [DllImport("libdl.dylib", EntryPoint = "dlsym")]
    private static extern IntPtr mac_dlsym(IntPtr handle, string symbol);

    [DllImport("libdl.dylib", EntryPoint = "dlclose")]
    private static extern int mac_dlclose(IntPtr handle);
    
    [DllImport("libdl.dylib", EntryPoint = "dlerror")]
    private static extern IntPtr mac_dlerror();
}