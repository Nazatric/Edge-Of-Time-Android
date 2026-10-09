/**
 * @file    android_jni.cpp
 * @brief   JavaVM/Context registration and per-thread JNIEnv management
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

#include <rex/android_jni.h>

#include <pthread.h>

#include <atomic>

namespace rex {
namespace android {
namespace {

JavaVM* g_vm = nullptr;
jobject g_context = nullptr;  // global ref

pthread_key_t g_detach_key;
std::atomic<bool> g_detach_key_ready{false};

/// Destructor for the TLS key: detaches threads this layer attached, so a
/// worker thread exiting does not leak a JNI attachment.
void DetachCurrentThread(void* value) {
  if (value && g_vm) {
    g_vm->DetachCurrentThread();
  }
}

void EnsureDetachKey() {
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, [] {
    if (pthread_key_create(&g_detach_key, &DetachCurrentThread) == 0) {
      g_detach_key_ready.store(true, std::memory_order_release);
    }
  });
}

}  // namespace

bool RegisterJni(JavaVM* vm, jobject context) {
  if (!vm || !context) {
    return false;
  }
  g_vm = vm;
  EnsureDetachKey();

  JNIEnv* env = GetThreadEnv();
  if (!env) {
    g_vm = nullptr;
    return false;
  }
  if (g_context) {
    env->DeleteGlobalRef(g_context);
    g_context = nullptr;
  }
  g_context = env->NewGlobalRef(context);
  return g_context != nullptr;
}

void UnregisterJni() {
  if (g_context) {
    if (JNIEnv* env = GetThreadEnv()) {
      env->DeleteGlobalRef(g_context);
    }
    g_context = nullptr;
  }
  g_vm = nullptr;
}

JNIEnv* GetThreadEnv() {
  if (!g_vm) {
    return nullptr;
  }
  JNIEnv* env = nullptr;
  const jint status = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (status == JNI_OK) {
    return env;
  }
  if (status != JNI_EDETACHED) {
    return nullptr;
  }
  if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
    return nullptr;
  }
  // Mark this thread so the TLS destructor detaches it on exit.
  if (g_detach_key_ready.load(std::memory_order_acquire)) {
    pthread_setspecific(g_detach_key, env);
  }
  return env;
}

jobject GetContext() { return g_context; }

}  // namespace android
}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
