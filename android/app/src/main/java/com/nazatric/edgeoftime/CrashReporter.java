package com.nazatric.edgeoftime;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Captures uncaught Java exceptions to {@code filesDir/crashes/} and to logcat.
 *
 * <p>Native crashes are handled separately by the platform: Android writes
 * tombstones to {@code /data/tombstones/} (readable via {@code adb bugreport}
 * or on-device developer settings). This class covers the Java side and also
 * records device/app metadata alongside each crash to make reports actionable.
 */
public final class CrashReporter implements Thread.UncaughtExceptionHandler {
    private static final String TAG = "EdgeOfTimeCrash";
    private static final SimpleDateFormat STAMP =
            new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US);

    private static final Object LOCK = new Object();
    private static Context appContext;
    private static Thread.UncaughtExceptionHandler previous;
    private static boolean installed;

    private CrashReporter() {}

    public static synchronized void install(Context context) {
        if (installed) {
            return;
        }
        appContext = context.getApplicationContext();
        previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new CrashReporter());
        installed = true;
    }

    @Override
    public void uncaughtException(Thread thread, Throwable throwable) {
        try {
            writeCrashFile(thread, throwable);
        } catch (Throwable ignored) {
            // Never let the crash reporter itself crash.
        }
        Log.e(TAG, "Uncaught exception in thread " + thread.getName(), throwable);
        if (previous != null) {
            previous.uncaughtException(thread, throwable);
        }
    }

    private static void writeCrashFile(Thread thread, Throwable throwable) {
        if (appContext == null) {
            return;
        }
        File dir = new File(appContext.getFilesDir(), "crashes");
        if (!dir.exists() && !dir.mkdirs()) {
            return;
        }
        File file = new File(dir, "crash-" + STAMP.format(new Date()) + ".txt");
        try (FileWriter fw = new FileWriter(file);
             PrintWriter pw = new PrintWriter(fw)) {
            pw.println("thread: " + thread.getName());
            pw.println("time: " + new Date());
            pw.println("device: " + Build.MANUFACTURER + " " + Build.MODEL
                    + " (SDK " + Build.VERSION.SDK_INT + ", " + Build.VERSION.RELEASE + ")");
            pw.println("abi: " + Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "?");
            try {
                PackageInfo pi = appContext.getPackageManager().getPackageInfo(
                        appContext.getPackageName(), 0);
                pw.println("app: " + pi.packageName + " versionName=" + pi.versionName
                        + " versionCode=" + pi.versionCode);
            } catch (PackageManager.NameNotFoundException ignored) {
            }
            pw.println();
            StringWriter sw = new StringWriter();
            throwable.printStackTrace(new PrintWriter(sw));
            pw.println(sw);
        } catch (Throwable ignored) {
        }
    }
}
