#pragma once
/**
 * @file    rex/android_jni.h
 * @brief   JNI handle registration for the Android platform layer
 *
 * The SDK's Android filesystem path needs to call into the Java
 * ContentResolver to resolve content:// URIs handed over by the Storage Access
 * Framework. Rather than making rexcore depend on SDL (which owns the Activity
 * in this port), the application layer registers the JavaVM and its Context
 * once at startup and the native code uses them from there.
 *
 * Call rex::android::RegisterJni() exactly once, before any filesystem use.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <jni.h>

namespace rex {
namespace android {

/// Registers the process JavaVM and an application Context.
///
/// @param vm       the process JavaVM; must outlive all native code.
/// @param context  any android.content.Context (the Activity or the
///                 Application). A global reference is taken internally, so
///                 the caller may release its own reference afterwards.
/// @return true if both handles were accepted.
bool RegisterJni(JavaVM* vm, jobject context);

/// Releases the global Context reference taken by RegisterJni.
void UnregisterJni();

/// Returns a JNIEnv for the calling thread, attaching it to the VM if needed.
/// Returns nullptr if RegisterJni has not been called or attaching failed.
///
/// Threads attached by this call are detached automatically when they exit.
JNIEnv* GetThreadEnv();

/// Returns the global Context reference, or nullptr if unregistered.
jobject GetContext();

}  // namespace android
}  // namespace rex
