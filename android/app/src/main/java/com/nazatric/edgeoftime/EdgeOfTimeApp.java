package com.nazatric.edgeoftime;

import android.app.Application;
import android.content.Context;

/**
 * Application object. Installs the crash reporter as early as possible so that
 * Java-side crashes are captured to a file the user can send back.
 */
public class EdgeOfTimeApp extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        CrashReporter.install(base);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        CrashReporter.install(getApplicationContext());
    }
}
