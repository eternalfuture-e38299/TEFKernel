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

namespace tefloader;

public class Test
{
    private static Test i = new();
    private int bb = 1114514;
    private static int Hello(int a)
    {
        Logger.Info($"Hello! {a}");
        return 0;
    }

    private delegate void TestD();
    public static void Main()
    {
        Program.TefKernelLib.LoadLib("/home/eternalfuture/开源项目/TEFKernel/cmake-build-debug/libtefkernel.so");
        Logger.Initialize(Program.TefKernelLib);
        NetApi.Initialization.MethodInit.Init();
        NetApi.Initialization.TypeApi.Init();
        NetApi.Initialization.FieldApi.Init();
        NetApi.Initialization.PropertyApi.Init();
        NetApi.Initialization.StructInit.InitAll();

        
        var t = Marshal.GetDelegateForFunctionPointer<TestD>(Program.TefKernelLib.GetSym("init_tefkernel"));
        t();
    }
}