using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

internal static class GpuTest
{
    private const ulong CL_DEVICE_TYPE_GPU = 4;
    private const uint CL_MEM_READ_WRITE = 1;
    private const uint CL_DEVICE_NAME = 0x102B;
    private const uint CL_PROGRAM_BUILD_LOG = 0x1183;
    private const int CL_SUCCESS = 0;

    [DllImport("OpenCL.dll")]
    private static extern int clGetPlatformIDs(uint num, IntPtr[] platforms, out uint count);

    [DllImport("OpenCL.dll")]
    private static extern int clGetDeviceIDs(IntPtr platform, ulong type, uint num, IntPtr[] devices, out uint count);

    [DllImport("OpenCL.dll")]
    private static extern IntPtr clCreateContext(IntPtr props, uint num, IntPtr[] devices, IntPtr pfn, IntPtr user, out int err);

    [DllImport("OpenCL.dll")]
    private static extern IntPtr clCreateCommandQueue(IntPtr ctx, IntPtr dev, ulong props, out int err);

    [DllImport("OpenCL.dll", CharSet = CharSet.Ansi)]
    private static extern IntPtr clCreateProgramWithSource(IntPtr ctx, uint count, string[] src, IntPtr lengths, out int err);

    [DllImport("OpenCL.dll", CharSet = CharSet.Ansi)]
    private static extern int clBuildProgram(IntPtr prog, uint num, IntPtr[] devices, string options, IntPtr pfn, IntPtr user);

    [DllImport("OpenCL.dll", CharSet = CharSet.Ansi)]
    private static extern IntPtr clCreateKernel(IntPtr prog, string name, out int err);

    [DllImport("OpenCL.dll")]
    private static extern IntPtr clCreateBuffer(IntPtr ctx, ulong flags, IntPtr size, IntPtr host, out int err);

    [DllImport("OpenCL.dll")]
    private static extern int clSetKernelArg(IntPtr kernel, uint index, IntPtr size, ref IntPtr value);

    [DllImport("OpenCL.dll")]
    private static extern int clEnqueueNDRangeKernel(IntPtr queue, IntPtr kernel, uint dims, IntPtr[] offsets, IntPtr[] global, IntPtr[] local, uint wait, IntPtr[] events, IntPtr ev);

    [DllImport("OpenCL.dll")]
    private static extern int clFinish(IntPtr queue);

    [DllImport("OpenCL.dll")]
    private static extern int clGetDeviceInfo(IntPtr dev, uint param, IntPtr size, byte[] value, out IntPtr sizeRet);

    [DllImport("OpenCL.dll")]
    private static extern int clGetProgramBuildInfo(IntPtr prog, IntPtr dev, uint param, IntPtr size, byte[] value, out IntPtr sizeRet);

    private static string DeviceName(IntPtr dev)
    {
        IntPtr size;
        if (clGetDeviceInfo(dev, CL_DEVICE_NAME, IntPtr.Zero, null, out size) != CL_SUCCESS || size.ToInt64() <= 1)
        {
            return "";
        }
        byte[] buf = new byte[size.ToInt64()];
        if (clGetDeviceInfo(dev, CL_DEVICE_NAME, size, buf, out size) != CL_SUCCESS)
        {
            return "";
        }
        return Encoding.ASCII.GetString(buf).TrimEnd('\0');
    }

    private static string BuildLog(IntPtr prog, IntPtr dev)
    {
        IntPtr size;
        if (clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, IntPtr.Zero, null, out size) != CL_SUCCESS || size.ToInt64() <= 1)
        {
            return "";
        }
        byte[] buf = new byte[size.ToInt64()];
        if (clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, size, buf, out size) != CL_SUCCESS)
        {
            return "";
        }
        return Encoding.ASCII.GetString(buf).TrimEnd('\0');
    }

    private static int Fail(string stage, int err)
    {
        Console.WriteLine("[-] OpenCL " + stage + " failed: " + err);
        return 3;
    }

    public static int Main(string[] args)
    {
        int seconds = 30;
        if (args.Length > 0)
        {
            int.TryParse(args[0], out seconds);
        }
        if (seconds <= 0) seconds = 30;

        uint platformCount;
        if (clGetPlatformIDs(0, null, out platformCount) != CL_SUCCESS || platformCount == 0)
        {
            return Fail("clGetPlatformIDs", -1);
        }
        IntPtr[] platforms = new IntPtr[platformCount];
        if (clGetPlatformIDs(platformCount, platforms, out platformCount) != CL_SUCCESS)
        {
            return Fail("clGetPlatformIDs", -2);
        }

        IntPtr device = IntPtr.Zero;
        string deviceName = "";
        for (int i = 0; i < platforms.Length; i++)
        {
            uint deviceCount;
            if (clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, null, out deviceCount) != CL_SUCCESS || deviceCount == 0)
            {
                continue;
            }
            IntPtr[] devices = new IntPtr[deviceCount];
            if (clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, deviceCount, devices, out deviceCount) != CL_SUCCESS)
            {
                continue;
            }
            for (int j = 0; j < devices.Length; j++)
            {
                string name = DeviceName(devices[j]);
                if (device == IntPtr.Zero || name.IndexOf("NVIDIA", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    device = devices[j];
                    deviceName = name;
                    if (name.IndexOf("NVIDIA", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        break;
                    }
                }
            }
            if (device != IntPtr.Zero && deviceName.IndexOf("NVIDIA", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                break;
            }
        }

        if (device == IntPtr.Zero)
        {
            Console.WriteLine("[-] no OpenCL GPU device");
            return 4;
        }
        Console.WriteLine("[+] GPU: " + deviceName);

        int err;
        IntPtr ctx = clCreateContext(IntPtr.Zero, 1, new IntPtr[] { device }, IntPtr.Zero, IntPtr.Zero, out err);
        if (ctx == IntPtr.Zero)
        {
            return Fail("clCreateContext", err);
        }
        IntPtr queue = clCreateCommandQueue(ctx, device, 0, out err);
        if (queue == IntPtr.Zero)
        {
            return Fail("clCreateCommandQueue", err);
        }

        string source = @"
__kernel void burn(__global float *out, const float seed) {
    float x = seed + (float)get_global_id(0);
    for (int i = 0; i < 50000; i++) {
        x = fma(x, 1.000001f, 0.0000001f);
    }
    out[get_global_id(0)] = x;
}
";
        string[] sources = new string[] { source };
        IntPtr program = clCreateProgramWithSource(ctx, 1, sources, IntPtr.Zero, out err);
        if (program == IntPtr.Zero)
        {
            return Fail("clCreateProgramWithSource", err);
        }
        err = clBuildProgram(program, 0, null, "", IntPtr.Zero, IntPtr.Zero);
        if (err != CL_SUCCESS)
        {
            Console.WriteLine("[-] build log: " + BuildLog(program, device));
            return Fail("clBuildProgram", err);
        }

        IntPtr kernel = clCreateKernel(program, "burn", out err);
        if (kernel == IntPtr.Zero)
        {
            return Fail("clCreateKernel", err);
        }

        IntPtr buffer = clCreateBuffer(ctx, CL_MEM_READ_WRITE, (IntPtr)(4 * 1024 * 1024), IntPtr.Zero, out err);
        if (buffer == IntPtr.Zero)
        {
            return Fail("clCreateBuffer", err);
        }
        IntPtr bufferArg = buffer;
        clSetKernelArg(kernel, 0, (IntPtr)IntPtr.Size, ref bufferArg);
        IntPtr seed = Marshal.AllocHGlobal(4);
        Marshal.WriteInt32(seed, BitConverter.ToInt32(BitConverter.GetBytes(1.0f), 0));
        clSetKernelArg(kernel, 1, (IntPtr)4, ref seed);

        IntPtr[] global = new IntPtr[] { (IntPtr)(1024 * 1024) };
        Stopwatch sw = Stopwatch.StartNew();
        long dispatches = 0;
        while (sw.Elapsed.TotalSeconds < seconds)
        {
            err = clEnqueueNDRangeKernel(queue, kernel, 1, null, global, null, 0, null, IntPtr.Zero);
            if (err != CL_SUCCESS)
            {
                return Fail("clEnqueueNDRangeKernel", err);
            }
            clFinish(queue);
            dispatches++;
        }
        Console.WriteLine("[+] dispatches: " + dispatches);
        return 0;
    }
}
