package eternal.future.tefkernel;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;

public class Tefloader {
    private static final String TAG = "Tefloader";

    // Content Provider constants
    private static final String AUTHORITIES = "eternal.future.tefkernel.fileprovider";
    private static final String METHOD_GET_EXTERNAL_DIR = "getExternalDir";
    private static final String METHOD_OPEN = "open";
    private static final String METHOD_ACCESS = "access";

    // Bundle keys
    private static final String KEY_SUCCESS = "success";
    private static final String KEY_DIR_PATH = "dir_path";
    private static final String KEY_FD = "fd";
    private static final String KEY_PATH = "path";
    private static final String KEY_MODE = "mode";

    // Kernel file constants
    private static final String KERNEL_BASE_NAME = "libtefkernel";
    private static final String KERNEL_FILE_EXTENSION = ".so";
    private static final String ARM64_V8A = "arm64-v8a";
    private static final String ARMEABI_V7A = "armeabi-v7a";
    private static final String KERNEL_SUB_DIR = "tefkernel";

    @SuppressLint("UnsafeDynamicallyLoadedCode")
    public static void initTefKernel(Context context) {
        Log.i(TAG, "Tefloader.initTefKernel() started");

        try {
            // Step 1: Detect architecture
            Log.d(TAG, "Detecting device architecture...");
            String arch = getCurrentApplicationArchitecture();
            Log.i(TAG, "Detected architecture: " + arch);

            if (!arch.equals(ARM64_V8A) && !arch.equals(ARMEABI_V7A)) {
                Log.e(TAG, "Unsupported architecture: " + arch);
                return;
            }

            // Step 2: Get kernel file path
            Log.d(TAG, "Getting kernel file path...");
            String kernelPath = getKernelPath(context, arch);
            if (kernelPath == null) {
                Log.e(TAG, "Failed to get kernel file path");
                return;
            }
            Log.i(TAG, "Kernel file found: " + kernelPath);

            // Step 3: Get file descriptor via Content Provider
            Log.d(TAG, "Getting file descriptor...");
            ParcelFileDescriptor kernelFd = getFileDescriptor(context, kernelPath);
            if (kernelFd == null) {
                Log.e(TAG, "Failed to get file descriptor");
                return;
            }

            try {
                // Step 4: Copy file to app private directory
                Log.d(TAG, "Copying kernel to cache...");
                File copiedFile = copyKernelToCache(context, arch, kernelFd);

                if (copiedFile == null || !copiedFile.exists()) {
                    Log.e(TAG, "File copy failed");
                    return;
                }
                Log.i(TAG, "File copied successfully, size: " + copiedFile.length() + " bytes");

                // Step 5: Load the kernel library
                Log.d(TAG, "Loading kernel library...");
                long loadStartTime = System.currentTimeMillis();
                System.load(copiedFile.getAbsolutePath());
                long loadEndTime = System.currentTimeMillis();

                Log.i(TAG, "TefKernel loaded successfully in " + (loadEndTime - loadStartTime) + "ms");
                Log.i(TAG, "Tefloader initialization completed");

            } finally {
                if (kernelFd != null) {
                    try {
                        kernelFd.close();
                    } catch (IOException e) {
                        Log.w(TAG, "Error closing file descriptor: " + e.getMessage());
                    }
                }
            }

        } catch (Exception e) {
            Log.e(TAG, "Tefloader initialization failed", e);
        }
    }

    private static String getCurrentApplicationArchitecture() {
        String detectedArch = ARM64_V8A;

        try {
            Process process = Runtime.getRuntime().exec("cat /proc/self/maps");
            java.io.BufferedReader reader = new java.io.BufferedReader(
                new java.io.InputStreamReader(process.getInputStream()));

            String line;
            while ((line = reader.readLine()) != null) {
                if (line.contains(ARM64_V8A)) {
                    detectedArch = ARM64_V8A;
                    break;
                }
                if (line.contains(ARMEABI_V7A)) {
                    detectedArch = ARMEABI_V7A;
                    break;
                }
            }
            reader.close();
            process.waitFor();

        } catch (Exception e) {
            // Fallback to system property
            String archProperty = System.getProperty("os.arch", "unknown");
            if (archProperty.contains("aarch64") || archProperty.contains("arm64")) {
                detectedArch = ARM64_V8A;
            } else if (archProperty.contains("arm")) {
                detectedArch = ARMEABI_V7A;
            }
        }

        return detectedArch;
    }

    private static String getKernelPath(Context context, String arch) {
        String dirPath = getDirectoryPath(context, METHOD_GET_EXTERNAL_DIR);
        if (dirPath == null) {
            return null;
        }

        File kernelFile = new File(new File(dirPath, KERNEL_SUB_DIR),
                                  KERNEL_BASE_NAME + "." + arch + KERNEL_FILE_EXTENSION);

        String kernelPath = kernelFile.getAbsolutePath();

        // Check if file exists via Content Provider
        if (!checkFileExistsViaProvider(context, kernelPath)) {
            Log.e(TAG, "Kernel file not found: " + kernelPath);
            return null;
        }

        return kernelPath;
    }

    private static boolean checkFileExistsViaProvider(Context context, String filePath) {
        try {
            Bundle params = new Bundle();
            params.putString(KEY_PATH, filePath);

            Bundle result = context.getContentResolver().call(
                Uri.parse("content://" + AUTHORITIES),
                METHOD_ACCESS,
                null,
                params);

            return result != null && result.getBoolean(KEY_SUCCESS, false);

        } catch (Exception e) {
            Log.e(TAG, "Error checking file existence", e);
            return false;
        }
    }

    private static String getDirectoryPath(Context context, String method) {
        try {
            Bundle result = context.getContentResolver().call(
                Uri.parse("content://" + AUTHORITIES),
                method,
                null,
                null);

            if (result == null) {
                return null;
            }

            boolean success = result.getBoolean(KEY_SUCCESS, false);
            if (success) {
                return result.getString(KEY_DIR_PATH);
            }

        } catch (Exception e) {
            Log.e(TAG, "Error getting directory path", e);
        }
        return null;
    }

    private static ParcelFileDescriptor getFileDescriptor(Context context, String filePath) {
        try {
            Bundle params = new Bundle();
            params.putString(KEY_PATH, filePath);
            params.putString(KEY_MODE, "r");

            Bundle result = context.getContentResolver().call(
                Uri.parse("content://" + AUTHORITIES),
                METHOD_OPEN,
                null,
                params);

            if (result == null) {
                return null;
            }

            boolean success = result.getBoolean(KEY_SUCCESS, false);
            if (success) {
                return result.getParcelable(KEY_FD);
            }

        } catch (Exception e) {
            Log.e(TAG, "Error getting file descriptor", e);
        }
        return null;
    }

    private static File copyKernelToCache(Context context, String arch, ParcelFileDescriptor kernelFd) throws IOException {
        File cacheDir = context.getFilesDir();
        File kernelFile = new File(cacheDir, "libtefkernel." + arch + ".so");

        // Delete old file if exists
        if (kernelFile.exists()) {
            kernelFile.delete();
        }

        try (FileInputStream fis = new FileInputStream(kernelFd.getFileDescriptor());
             FileOutputStream fos = new FileOutputStream(kernelFile);
             FileChannel source = fis.getChannel();
             FileChannel dest = fos.getChannel()) {

            long transferred = dest.transferFrom(source, 0, source.size());

            if (kernelFile.exists() && kernelFile.length() > 0) {
                return kernelFile;
            }

        } catch (IOException e) {
            if (kernelFile.exists()) {
                kernelFile.delete();
            }
            throw e;
        }

        return null;
    }
}
