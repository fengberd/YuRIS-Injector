﻿using System;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace YuRISInjector
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            StartInjectProcess(Path.GetFullPath("nekonin.exe"), "", null, Path.GetFullPath("YuRIS-Hook.dll"));
        }

        public static void StartInjectProcess(string filename, string args, string workDir, string dll)
        {
            var oldEnv = Environment.CurrentDirectory;
            Environment.CurrentDirectory = Path.GetDirectoryName(filename);
            try
            {
                var startInfo = new STARTUPINFO();
                if (!NTAPI.CreateProcess(filename, args, IntPtr.Zero, IntPtr.Zero, false, NTAPI.CREATE_SUSPENDED, IntPtr.Zero, workDir, ref startInfo, out PROCESS_INFORMATION process))
                {
                    MessageBox.Show("CreateProcess failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var thread = new CONTEXT()
                {
                    ContextFlags = CONTEXT_FLAGS.CONTEXT_CONTROL | CONTEXT_FLAGS.CONTEXT_INTEGER
                };
                if (!NTAPI.GetThreadContext(process.hThread, ref thread))
                {
                    MessageBox.Show("GetThreadContext failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                IntPtr loadLibrary = NTAPI.GetProcAddress(NTAPI.GetModuleHandle("kernel32.dll"), "LoadLibraryW");
                if (loadLibrary == IntPtr.Zero)
                {
                    MessageBox.Show("GetProcAddress or GetModuleHandle failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var buffer = NTAPI.VirtualAllocEx(process.hProcess, IntPtr.Zero, 4 * 1024, AllocationType.Commit | AllocationType.Reserve, MemoryProtection.ExecuteReadWrite);
                if (buffer == IntPtr.Zero)
                {
                    MessageBox.Show("VirtualAllocEx failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                // Write DLL Path
                byte[] tmp = Encoding.GetEncoding("UTF-16").GetBytes(dll);
                Array.Resize(ref tmp, tmp.Length + 1);
                tmp[tmp.Length - 1] = 0x00;

                if (!NTAPI.WriteProcessMemory(process.hProcess, buffer, tmp, tmp.Length, out int written) || written != tmp.Length)
                {
                    MessageBox.Show("WriteProcessMemory(DllPath) failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                long returnAddress = thread.Eip;
                thread.Eip = (uint)(((uint)buffer + tmp.Length) / 16 + 3) * 16;

                // Construct shell code
                var shellcode = new byte[]
                {
                0x50, // push eax
                0xb8, 0x00, 0x00, 0x00, 0x00, // mov eax, <Buffer>
                0x50, // push eax
                0xb8, 0x00, 0x00, 0x00, 0x00, // mov eax, <LoadLibraryW>
                0xff, 0xd0, // call eax
                0x58, // pop eax
                0xe9, 0x00, 0x00, 0x00, 0x00 // jmp <returnAddress>
                };

                Array.Copy(BitConverter.GetBytes((int)buffer), 0, shellcode, 2, 4);
                Array.Copy(BitConverter.GetBytes((int)loadLibrary), 0, shellcode, 8, 4);
                Array.Copy(BitConverter.GetBytes(returnAddress - thread.Eip - shellcode.Length), 0, shellcode, shellcode.Length - 4, 4);

                if (!NTAPI.WriteProcessMemory(process.hProcess, (IntPtr)thread.Eip, shellcode, shellcode.Length, out written) || written != shellcode.Length)
                {
                    MessageBox.Show("WriteProcessMemory(ShellCode) failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                if (!NTAPI.SetThreadContext(process.hThread, ref thread))
                {
                    MessageBox.Show("SetThreadContext failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                if (!NTAPI.ResumeThread(process.hThread))
                {
                    MessageBox.Show("ResumeThread failed: " + Marshal.GetLastWin32Error(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
            }
            finally
            {
                Environment.CurrentDirectory = oldEnv;
            }
        }
    }
}
