package com.nazatric.edgeoftime;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.util.Log;

/**
 * Java side of the native bridge.
 *
 * <p>Native code (libedgeoftime.so) calls back into {@link #onNativeStatus} to
 * report launcher/renderer state, and Java calls into native through the
 * {@code native*} methods declared here. The game folder URI selected through
 * the Storage Access Framework is persisted across launches, and
 * {@link #probeGameFile(String)} resolves files inside it for the native
 * launcher using framework APIs only (no AndroidX).
 */
public final class NativeBridge {
    private static final String TAG = "EdgeOfTimeBridge";

    private static final Object LOCK = new Object();
    private static Context appContext;
    private static String gameFolderUri;
    private static StatusOverlay overlay;
    private static Handler mainHandler;

    private NativeBridge() {}

    // ------------------------------------------------------------------
    // Called from MainActivity (main thread).
    // ------------------------------------------------------------------

    public static void attach(Context context, StatusOverlay o) {
        synchronized (LOCK) {
            appContext = context.getApplicationContext();
            overlay = o;
            mainHandler = new Handler(Looper.getMainLooper());
        }
    }

    public static void detachOverlay() {
        synchronized (LOCK) {
            overlay = null;
        }
    }

    public static void setGameFolderUri(String uri) {
        synchronized (LOCK) {
            gameFolderUri = uri;
        }
        Log.i(TAG, "Game folder set: " + uri);
        nativeNotifyGameFolderChanged();
    }

    /** Returns the persisted SAF tree URI, or null if none was selected. */
    public static String getGameFolderUri() {
        synchronized (LOCK) {
            return gameFolderUri;
        }
    }

    // ------------------------------------------------------------------
    // Called from native code (SDL main thread / timer thread).
    // ------------------------------------------------------------------

    /** Native -> Java status callback. Safe to call from any thread. */
    public static void onNativeStatus(String message) {
        final StatusOverlay o;
        final Handler h;
        synchronized (LOCK) {
            o = overlay;
            h = mainHandler;
        }
        if (o == null || h == null) {
            return;
        }
        h.post(() -> o.setStatus(message));
    }

    /**
     * Resolves a '/'-separated relative path inside the selected SAF tree to a
     * full content:// document URI, or returns null when the file is absent.
     * Called from native code; performs a ContentResolver query per path
     * segment, so it is only used for a handful of probe files.
     */
    public static String probeGameFile(String relativePath) {
        final Context context;
        final String treeUriString;
        synchronized (LOCK) {
            context = appContext;
            treeUriString = gameFolderUri;
        }
        if (context == null || treeUriString == null || relativePath == null) {
            return null;
        }
        try {
            Uri tree = Uri.parse(treeUriString);
            String documentId = DocumentsContract.getTreeDocumentId(tree);
            for (String segment : relativePath.split("/")) {
                if (segment.isEmpty()) {
                    continue;
                }
                Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                        tree, documentId);
                String foundId = null;
                try (Cursor cursor = context.getContentResolver().query(
                        childrenUri,
                        new String[]{
                                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                                DocumentsContract.Document.COLUMN_DISPLAY_NAME
                        },
                        null, null, null)) {
                    if (cursor != null) {
                        while (cursor.moveToNext()) {
                            if (segment.equals(cursor.getString(1))) {
                                foundId = cursor.getString(0);
                                break;
                            }
                        }
                    }
                }
                if (foundId == null) {
                    return null;
                }
                documentId = foundId;
            }
            return DocumentsContract.buildDocumentUriUsingTree(tree, documentId).toString();
        } catch (Exception e) {
            Log.w(TAG, "probeGameFile failed for " + relativePath, e);
            return null;
        }
    }

    // ------------------------------------------------------------------
    // Java -> native entry points (implemented in libedgeoftime.so).
    // ------------------------------------------------------------------

    /** Wakes the native launcher so it re-probes for game files. */
    public static native void nativeNotifyGameFolderChanged();

    /** Asks the native side to shut down cleanly. */
    public static native void nativeRequestQuit();
}
