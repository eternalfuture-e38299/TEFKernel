// /*******************************************************************************
//  * tefkernel - Program.cs
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
using System.Runtime.InteropServices;
using HarmonyLib;
using Newtonsoft.Json.Linq;
using tefloader.Il2CppApi;
using static System.Reflection.Assembly;

namespace tefloader;

public abstract class Program
{
    private static string _kernelLibPath = string.Empty;
    private static string _exePath = "Terraria.exe";
    private static string _workDirs = string.Empty;

    public static readonly LibLoader TefKernelLib = new();
    private static InitAryDelegate? _initAry;

    public static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        
        for (var i = 0; i < args.Length; i++)
        {
            switch (args[i].ToLower())
            {
                case "--server":
                case "-server":
                    _exePath = "TerrariaServer.exe";
                    break;
                case "--workpath":
                case "-w":
                    if (i + 1 < args.Length)
                    {
                        _workDirs = args[++i];
                    }
                    break;
                case "--kernel":
                case "-k":
                    if (i + 1 < args.Length)
                    {
                        _kernelLibPath = args[++i];
                    }
                    break;
                case "--exe":
                case "-e":
                    if (i + 1 < args.Length)
                    {
                        _exePath = args[++i];
                    }
                    break;
                case "--help":
                case "-h":
                    ShowHelp();
                    return;
            }
        }

        // 确保工作路径存在
        if (!Directory.Exists(_workDirs))
        {
            Console.WriteLine("Unable to find the working directory");
            Environment.Exit(1);
        }
        
        // 检查kernel库文件是否存在
        if (!File.Exists(_kernelLibPath))
        {
            Console.WriteLine($"ERROR: TefKernel library not found at '{_kernelLibPath}'");
            Console.WriteLine("Please specify the correct path using --kernel parameter.");
            Console.WriteLine($"Current work directory: {Directory.GetCurrentDirectory()}");
            Environment.Exit(1);
        }

        Console.WriteLine($"Using TefKernel: {Path.GetFullPath(_kernelLibPath)}");
        Console.WriteLine($"Using executable: {_kernelLibPath}");

        try
        {
            TefKernelLib.LoadLib(_kernelLibPath);
            Console.WriteLine($"Successfully loaded TefKernel from: {_kernelLibPath}");
        }
        catch (Exception ex) when (ex is DllNotFoundException or FileNotFoundException)
        {
            Console.WriteLine($"ERROR: Failed to load TefKernel library from '{_kernelLibPath}'");
            Console.WriteLine($"Error details: {ex.Message}");
            Console.WriteLine("Please ensure the library file is not corrupted and is compatible with your system.");
            Environment.Exit(1);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"ERROR: Unexpected error while loading TefKernel library: {ex.Message}");
            Console.WriteLine("Please check system dependencies and permissions.");
            Environment.Exit(1);
        }

        try
        {
            // 验证必要的符号是否存在
            var initSymbol = TefKernelLib.GetSym("init_tefkernel");
            if (initSymbol == IntPtr.Zero)
                throw new Exception("Symbol 'init_tefkernel' not found");
            _initAry = Marshal.GetDelegateForFunctionPointer<InitAryDelegate>(initSymbol);
            Console.WriteLine("Successfully initialized TefKernel symbols");
        }
        catch (Exception ex)
        {
            Console.WriteLine("ERROR: Failed to get symbol 'init_tefkernel' from TefKernel library");
            Console.WriteLine($"Error details: {ex.Message}");
            Console.WriteLine("The kernel library may be incompatible or corrupted.");
            Environment.Exit(1);
        }

        // 传递参数到启动函数
        Launch([]);
    }

    private static void ShowHelp()
    {
        Console.WriteLine("Usage: TefLoader [options]");
        Console.WriteLine();
        Console.WriteLine("Options:");
        Console.WriteLine("  --server, -server           Run in server mode (TerrariaServer.exe)");
        Console.WriteLine("  --workpath, -w <path>       Set working directory path");
        Console.WriteLine("  --kernel, -k <path>         Specify TefKernel library path");
        Console.WriteLine("  --exe, -e <path>            Specify game executable path");
        Console.WriteLine("  --help, -h                  Show this help message");
        Console.WriteLine();
        Console.WriteLine("Examples:");
        Console.WriteLine("  TefLoader                                   # Client mode, default workpath");
        Console.WriteLine("  TefLoader --server                          # Server mode, default workpath");
        Console.WriteLine("  TefLoader -w ./my_server                    # Set custom workpath");
        Console.WriteLine("  TefLoader --server -w /path/to/server       # Server with custom workpath");
        Console.WriteLine("  TefLoader -k ./custom/libtefkernel.so       # Client with custom kernel");
        Console.WriteLine("  TefLoader --server -w ./server -k lib.so    # Full server setup");
        Console.WriteLine();
        Console.WriteLine("Default paths:");
        Console.WriteLine("  Client mode:  workpath='./workspace', kernel='libtefkernel.so', exe='Terraria.exe'");
        Console.WriteLine("  Server mode:  workpath='./server_workspace', kernel='libtefkernel_server.so', exe='TerrariaServer.exe'");
        Console.WriteLine("  (If default kernel not found, falls back to 'libtefkernel.so')");
    }

    private static void LoadEmbeddedDependencies()
    {
        try
        {
            Logger.Info("Checking for embedded dependencies...");

            var jsonPath = Path.Combine(Path.GetDirectoryName(_exePath) ?? string.Empty, "Newtonsoft.Json.dll");
            if (File.Exists(jsonPath))
                try
                {
                    LoadFrom(jsonPath);
                    Logger.Info("Loaded Newtonsoft.Json from file system");
                }
                catch (Exception ex)
                {
                    Logger.Warning($"Failed to load Newtonsoft.Json from file: {ex}");
                }

            var assembly = LoadFrom(_exePath);
            var resourceNames = assembly.GetManifestResourceNames()
                .Where(name => name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                .ToList();

            if (resourceNames.Count == 0)
            {
                Logger.Debug("No embedded DLL resources found");
                return;
            }

            Logger.Info($"Found {resourceNames.Count} embedded DLL resources");

            // 创建一个字典来存储加载的程序集
            var loadedAssemblies = new Dictionary<string, Assembly>();

            foreach (var resourceName in resourceNames)
                try
                {
                    Logger.Debug($"Processing embedded DLL: {resourceName}");

                    using var stream = assembly.GetManifestResourceStream(resourceName);
                    if (stream == null)
                    {
                        Logger.Warning($"Failed to get stream for resource: {resourceName}");
                        continue;
                    }

                    var assemblyData = new byte[stream.Length];
                    // ReSharper disable once MustUseReturnValue
                    stream.Read(assemblyData, 0, assemblyData.Length);

                    var loadedAssembly = Load(assemblyData) ??
                                         throw new ArgumentNullException($"{nameof(Assembly)}.Load(assemblyData)");
                    Logger.Info($"Successfully loaded embedded assembly: {loadedAssembly.FullName}");

                    {
                        var assemblyName = new AssemblyName(loadedAssembly.FullName).Name;
                        if (assemblyName != null) loadedAssemblies[assemblyName] = loadedAssembly;
                    }

                    if (loadedAssembly.FullName.Contains("Newtonsoft.Json"))
                        Logger.Info($"Loaded Newtonsoft.Json v{loadedAssembly.GetName().Version}");
                }
                catch (Exception ex)
                {
                    Logger.Error($"Failed to load embedded DLL {resourceName}: {ex}");
                }

            // 注册所有加载的程序集到当前应用程序域
            RegisterLoadedAssemblies(loadedAssemblies);
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to load embedded dependencies: {ex}");
        }
    }

    private static void RegisterLoadedAssemblies(Dictionary<string, Assembly> loadedAssemblies)
    {
        try
        {
            // 获取当前应用程序域的私有方法来注册程序集
            var domain = AppDomain.CurrentDomain;
            var method = domain.GetType().GetMethod("InternalSetLoadedAssembly",
                BindingFlags.NonPublic | BindingFlags.Instance);

            if (method != null)
                foreach (var assembly in loadedAssemblies.Values)
                    try
                    {
                        method.Invoke(domain, [assembly]);
                        Logger.Debug($"Registered assembly: {assembly.FullName}");
                    }
                    catch (Exception ex)
                    {
                        Logger.Warning($"Failed to register assembly {assembly.FullName}: {ex}");
                    }
            else
                Logger.Warning("Could not find InternalSetLoadedAssembly method");
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to register loaded assemblies: {ex}");
        }
    }

    private static Assembly? CurrentDomain_AssemblyResolve(object sender, ResolveEventArgs args)
    {
        try
        {
            Logger.Debug($"Attempting to resolve assembly: {args.Name}");

            var assemblyName = new AssemblyName(args.Name);
            var simpleName = assemblyName.Name;

            if (simpleName == null)
                return null;

            var loadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
            foreach (var asm in loadedAssemblies)
            {
                if (new AssemblyName(asm.FullName).Name != simpleName) continue;
                Logger.Info($"Resolved {simpleName} from already loaded assemblies");
                return asm;
            }

            switch (simpleName)
            {
                case "Newtonsoft.Json":
                {
                    Logger.Info($"Resolving Newtonsoft.Json: {args.Name}");

                    var jsonPath = Path.Combine(Path.GetDirectoryName(_exePath) ?? string.Empty, "Newtonsoft.Json.dll");
                    if (File.Exists(jsonPath))
                        try
                        {
                            return LoadFrom(jsonPath);
                        }
                        catch (Exception ex)
                        {
                            Logger.Warning($"Failed to load Newtonsoft.Json from file: {ex}");
                        }

                    var targetAssembly = LoadFrom(_exePath);
                    var resourceName = targetAssembly.GetManifestResourceNames()
                        .FirstOrDefault(name => name.Contains("Newtonsoft.Json"));

                    if (resourceName != null)
                    {
                        using var stream = targetAssembly.GetManifestResourceStream(resourceName);
                        if (stream != null)
                        {
                            var assemblyData = new byte[stream.Length];
                            // ReSharper disable once MustUseReturnValue
                            stream.Read(assemblyData, 0, assemblyData.Length);
                            return Load(assemblyData);
                        }
                    }

                    break;
                }
                case "ReLogic" or "Ionic.Zip.CF" or "CsvHelper" or "NVorbis" or "MP3Sharp" or "Steamworks.NET"
                    or "RailSDK.Net":
                {
                    Logger.Info($"Resolving {simpleName} from embedded resources");

                    var targetAssembly = LoadFrom(_exePath);
                    var resourceName = targetAssembly.GetManifestResourceNames()
                        .FirstOrDefault(name => name.Contains(simpleName));

                    if (resourceName != null)
                    {
                        using var stream = targetAssembly.GetManifestResourceStream(resourceName);
                        if (stream != null)
                        {
                            var assemblyData = new byte[stream.Length];
                            // ReSharper disable once MustUseReturnValue
                            stream.Read(assemblyData, 0, assemblyData.Length);
                            return Load(assemblyData);
                        }
                    }

                    break;
                }
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Assembly resolve failed: {ex}");
        }

        return null;
    }


    private static void Launch(string[] args)
    {
        AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;
        Logger.Info($"Attempting to load assembly: {Path.GetFullPath(_exePath)}");

        if (!File.Exists(_exePath))
        {
            Logger.Error($"Error: File {_exePath} does not exist");
            Logger.Info($"Current directory: {Environment.CurrentDirectory}");
            return;
        }

        // Load embedded dependencies first
        LoadEmbeddedDependencies();

        // Verify the file before loading
        var fileInfo = new FileInfo(_exePath);
        Logger.Debug($"Target file size: {fileInfo.Length} bytes, Last modified: {fileInfo.LastWriteTimeUtc} UTC");

        var targetAssembly = LoadFrom(_exePath);
        Logger.Info($"Successfully loaded assembly: {targetAssembly.FullName}");

        if (targetAssembly == GetExecutingAssembly())
        {
            Logger.Error("Error: Attempted to load self instead of target assembly");
            Logger.Error("Please ensure Terraria_original.exe is the original game executable");
            Logger.Debug($"Loader assembly: {GetExecutingAssembly().FullName}");
            return;
        }

        var entryPoint = targetAssembly.EntryPoint;

        if (entryPoint == null)
        {
            Logger.Error("Error: Entry point (Main method) not found");
            Logger.Debug("Possible causes: corrupted executable or wrong file type");
            return;
        }

        if (entryPoint.DeclaringType != null)
        {
            Logger.Info($"Found entry point: {entryPoint.DeclaringType.FullName}.{entryPoint.Name}");
            Logger.Debug($"Module: {entryPoint.DeclaringType.Module.Name}");
        }

        object[]? parameters = entryPoint.GetParameters().Length > 0
            ? [args]
            : null;

        Logger.Info("Preparing to invoke entry point...");
        Logger.Debug($"Parameter count: {entryPoint.GetParameters().Length}");
        try
        {
            HookManager.Harmony.Patch(targetAssembly.GetType("Terraria.Program").GetMethod("SetupLogging",
                    BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance),
                postfix: new HarmonyMethod(typeof(Program), nameof(SetupLoggingPrefix)));
                

            entryPoint.Invoke(null, parameters);


            Logger.Info("Terraria main program has exited");
        }
        catch (BadImageFormatException bife)
        {
            Logger.Critical($"Invalid assembly format: {bife}");
            Logger.Debug("This usually indicates architecture mismatch (x86/x64) or corrupted file");
        }
        catch (FileLoadException fle)
        {
            Logger.Critical($"Assembly load failed: {fle}");
            if (fle.InnerException != null) Logger.Debug($"Inner exception: {fle.InnerException}");
        }
        catch (TargetInvocationException tie)
        {
            Logger.Critical($"Entry point invocation failed: {tie.InnerException ?? tie.GetBaseException()}");
            Logger.Debug($"Stack trace:\n{tie.InnerException?.StackTrace ?? tie.StackTrace}");
        }
        catch (Exception ex)
        {
            Logger.Critical($"Execution failed: {ex}");
            Logger.Error($"Message: {ex.Message}");
            Logger.Debug($"Stack trace:\n{ex.StackTrace}");
        }
    }


    [HarmonyPrefix]
    public static void SetupLoggingPrefix()
    {
        try
        {
            Initialization.RegisterAllApis();
            Logger.Info($"init_ary={_initAry!.Invoke(_workDirs)}");
        }
        catch (Exception ex)
        {
            Logger.Critical($"Execution failed: {ex}");
            Logger.Error($"Message: {ex.Message}");
            Logger.Debug($"Stack trace:\n{ex.StackTrace}");
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InitAryDelegate([MarshalAs(UnmanagedType.LPStr)] string workDirs);

    private delegate void TestD();
}
