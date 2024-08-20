package com.luoyesiqiu.shell;

import android.content.Context;
import android.util.Log;

import androidx.annotation.Keep;

import com.luoyesiqiu.shell.util.EnvUtils;

import java.io.File;

/**
 * Created by luoyesiqiu
 */
@Keep
public class JniBridge {
    private static final String TAG = "dpt_" + JniBridge.class.getSimpleName();
    public static native void craoc(String applicationClassName);
    public static native void craa(Context context, String applicationClassName);
    public static native void ia();
    public static native String rcf();
    public static native void mde(ClassLoader targetClassLoader);
    public static native void rde(ClassLoader classLoader,String elementName);
    public static native String gap();
    public static native String gdp();
    public static native Object ra(String originApplicationClassName);
    public static native String rapn();

    public static void loadShellLibs(String workspacePath,String apkPath) {
        final String[] allowLibNames = {Global.SHELL_SO_NAME};
        try {
            String abiDirName = EnvUtils.getAbiDirName(apkPath);
            File shellLibsFile = new File(workspacePath + File.separator + Global.LIB_DIR + File.separator + abiDirName);
            File[] files = shellLibsFile.listFiles();
            if(files != null) {
                for(File shellLibPath : files) {
                    String fullLibPath = shellLibPath.getAbsolutePath();
                    for(String libName : allowLibNames) {
                        String libSuffix = File.separator + libName;
                        if(fullLibPath.endsWith(libSuffix)) {
                            Log.d(TAG, "loadShellLibs: " + fullLibPath);
                            System.load(fullLibPath);
                        }
                    }
                }
            }
        }
        catch (Throwable e){
            Log.w(TAG,e);
        }
    }

}
