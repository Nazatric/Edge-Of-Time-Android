package com.nazatric.edgeoftime;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * Entry activity for the Edge of Time Android port.
 *
 * <p>Extends SDL3's {@link SDLActivity} (vendored under {@code org.libsdl.app},
 * zlib license) so that the whole SDL Android lifecycle - surface creation and
 * destruction, pause/resume, controller and HID device hotplug - is handled by
 * the same battle-tested glue that ships with SDL3 itself.
 *
 * <p>Native library load order matters: {@code rexruntime} statically contains
 * SDL3 and registers all of SDL's JNI entry points from its {@code JNI_OnLoad},
 * so it must be loaded first. {@code edgeoftime} (the launcher, which defines
 * {@code SDL_main}) is loaded last - SDLActivity derives the main library from
 * the last entry of {@link #getLibraries()}.
 */
public class MainActivity extends SDLActivity {
    private static final String TAG = "EdgeOfTime";
    private static final int REQUEST_GAME_FOLDER = 42;

    private StatusOverlay statusOverlay;

    @Override
    protected String[] getLibraries() {
        // SDL3 is statically linked into librexruntime.so, so "SDL3" is not
        // listed (there is no libSDL3.so in the APK).
        return new String[]{
                "rexruntime",
                "rexgpu-xenos",
                "edgeoftime",
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CrashReporter.install(getApplicationContext());
        statusOverlay = new StatusOverlay(this);
        NativeBridge.attach(this, statusOverlay);
        // SDLActivity.onCreate builds mLayout (a RelativeLayout) and calls
        // setContentView(mLayout); add our status overlay on top of the surface.
        if (mLayout != null) {
            mLayout.addView(statusOverlay.getView());
        }
        Log.i(TAG, "Edge of Time Android activity created (build " + BuildConfig.GIT_SHA + ")");
    }

    @Override
    protected void onDestroy() {
        NativeBridge.detachOverlay();
        super.onDestroy();
    }

    /** Launches the Storage Access Framework folder picker for the game files. */
    public void openGameFolderPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_GAME_FOLDER);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_GAME_FOLDER) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                Uri tree = data.getData();
                try {
                    getContentResolver().takePersistableUriPermission(
                            tree,
                            Intent.FLAG_GRANT_READ_URI_PERMISSION
                                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                } catch (SecurityException e) {
                    Log.w(TAG, "Could not persist URI permission for " + tree, e);
                }
                NativeBridge.setGameFolderUri(tree.toString());
            }
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
}
