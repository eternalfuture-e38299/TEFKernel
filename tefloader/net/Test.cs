// /*******************************************************************************
//  * tefkernel - Test.cs
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

using System.Data;
using System.Runtime.InteropServices;
using tefloader.Il2CppApi;

namespace tefloader;

public class Test
{
    private static Test i = new();
    private int bb = 1114514;
    private int Hello(int a)
    {
        Logger.Info($"Hello! {a}");
        return 0;
    }
    
    private static int HelloStatic(int a)
    {
        Logger.Info($"Hello! {a}");
        return 0;
    } 

    private delegate void TestD();
    public static void Main()
    {
        Program.TefKernelLib.LoadLib("/home/eternalfuture/开源项目/TEFKernel/cmake-build-debug/libtefkernel.linux.x64.so");
        Logger.Initialize(Program.TefKernelLib);
        
        Initialization.RegisterAllApis();
        
        var t = Marshal.GetDelegateForFunctionPointer<TestD>(Program.TefKernelLib.GetSym("init_tefkernel"));
        t();

        int staticResult = HelloStatic(1);
        Console.WriteLine($"HelloStatic(1) 返回值: {staticResult}");
        
        // 调用实例方法
        int instanceResult = i.Hello(2);
        Console.WriteLine($"i.Hello(2) 返回值: {instanceResult}");
    }
}