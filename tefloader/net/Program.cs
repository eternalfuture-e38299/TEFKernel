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
using System.Text;
using HarmonyLib;
using Newtonsoft.Json.Linq;
using static System.Reflection.Assembly;

namespace tefloader;

public abstract class Program
{
    private const string LaunchConfig = "tefloader-config.json";
    private static string _exePath = "Terraria.exe";
    private static string _workDirs = string.Empty;

    public static readonly LibLoader TefKernelLib = new();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InitAryDelegate([MarshalAs(UnmanagedType.LPStr)]string workDirs);
    private static InitAryDelegate? _initAry;
    
public static void Main(string[] args)
{
    Console.OutputEncoding = Encoding.UTF8;

    if (!File.Exists(LaunchConfig))
    {
        const string defaultJson = """

                                   {
                                     "kernelLibPath": "libtefkernel.so",
                                     "loadersPath": ".",
                                     "modsPath": "."
                                   }

                                   """;
        File.WriteAllText(LaunchConfig, defaultJson);
    }

    var jsonContent = File.ReadAllText(LaunchConfig);
    var config = JObject.Parse(jsonContent);

    string kernelLibPath = (string)config["kernelLibPath"]!;

    // Check if kernel library file exists
    if (!File.Exists(kernelLibPath))
    {
        Console.WriteLine($"ERROR: TefKernel library not found at '{kernelLibPath}'");
        Console.WriteLine("Please check the configuration file and ensure the library exists.");
        Environment.Exit(1);
    }

    try
    {
        TefKernelLib.LoadLib(kernelLibPath);
    }
    catch (Exception ex) when (ex is DllNotFoundException || ex is FileNotFoundException)
    {
        Console.WriteLine($"ERROR: Failed to load TefKernel library from '{kernelLibPath}'");
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

    _workDirs = (string)config["work_directory"]!;
    var exePath = config["exe_path"]?.Value<string>();
    if (exePath != null)
        _exePath = exePath;

    Logger.Initialize(TefKernelLib);
    NetApi.Initialization.InitializeAllApis();

    try
    {
        _initAry = Marshal.GetDelegateForFunctionPointer<InitAryDelegate>(TefKernelLib.GetSym("init_tefkernel"));
    }
    catch (Exception ex)
    {
        Console.WriteLine($"ERROR: Failed to get symbol 'init_tefkernel' from TefKernel library");
        Console.WriteLine($"Error details: {ex.Message}");
        Console.WriteLine("The kernel library may be incompatible or corrupted.");
        Environment.Exit(1);
    }

    Launch(args);
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
            Logger.Info($"init_ary={_initAry!.Invoke(_workDirs)}");
        }
        catch (Exception ex)
        {
            Logger.Critical($"Execution failed: {ex}");
            Logger.Error($"Message: {ex.Message}");
            Logger.Debug($"Stack trace:\n{ex.StackTrace}");
        }
    }
}
