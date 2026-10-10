/**
 * @file    filesystem_android.cpp
 * @brief   Storage Access Framework content:// resolution for the SDK
 *
 * rexglue-sdk/include/rex/filesystem.h declares
 * rex::filesystem::OpenAndroidContentFileDescriptor() and
 * src/core/mapped_memory_posix.cpp:137 calls it, but no implementation ships
 * with the SDK -- it is the last undefined symbol when linking librexruntime.so
 * for Android.
 *
 * This resolves a content:// URI through the Java ContentResolver and returns
 * a detached POSIX file descriptor the caller owns.
 *
 * @license BSD 3-Clause License (matches the ReXGlue SDK it plugs into)
 */

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

#include <rex/android_jni.h>
#include <rex/filesystem.h>

#include <string>

namespace rex {
namespace filesystem {
namespace {

/// Returns true if the given source string is a Storage Access Framework
/// content:// URI rather than a plain filesystem path.
bool LooksLikeContentUri(const std::string_view source) {
  return source.starts_with("content://");
}

/// RAII for a local JNI reference.
class LocalRef {
 public:
  LocalRef(JNIEnv* env, jobject obj) : env_(env), obj_(obj) {}
  ~LocalRef() {
    if (env_ && obj_) {
      env_->DeleteLocalRef(obj_);
    }
  }
  LocalRef(const LocalRef&) = delete;
  LocalRef& operator=(const LocalRef&) = delete;
  jobject get() const { return obj_; }
  explicit operator bool() const { return obj_ != nullptr; }

 private:
  JNIEnv* env_;
  jobject obj_;
};

/// Clears any pending JNI exception so later JNI calls are not poisoned.
bool ClearPendingException(JNIEnv* env) {
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return true;
  }
  return false;
}

/// Maps the SDK's fopen-style mode onto the modes ParcelFileDescriptor accepts.
/// ContentResolver.openFileDescriptor only understands "r", "w", "wt", "wa",
/// "rw" and "rwt".
const char* NormalizeMode(const char* mode) {
  if (!mode) {
    return "r";
  }
  const std::string m(mode);
  if (m == "rw" || m == "w" || m == "wt" || m == "wa" || m == "rwt") {
    return mode;
  }
  // Anything else (including "rb", "r") degrades to read-only, which is what
  // the SDK uses for mapping game data.
  return "r";
}

}  // namespace

bool IsAndroidContentUri(const std::string_view source) { return LooksLikeContentUri(source); }

void AndroidInitialize() {
  // The filesystem layer itself needs no global state; the JNI registration it
  // relies on is performed by the application entry point via
  // rex::android::RegisterJni(). This hook exists so platform bring-up has a
  // single, ordered call site (mirroring rex::memory::AndroidInitialize and
  // rex::thread::AndroidInitialize).
}

void AndroidShutdown() {}

int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode) {
  JNIEnv* env = rex::android::GetThreadEnv();
  jobject context = rex::android::GetContext();
  if (!env || !context) {
    // RegisterJni was never called -- the app layer must do this at startup.
    return -1;
  }

  // Ensure we have room for the locals created below even on a minimal frame.
  if (env->PushLocalFrame(16) != JNI_OK) {
    ClearPendingException(env);
    return -1;
  }

  int result_fd = -1;
  do {
    // Uri.parse(uri)
    jclass uri_class = env->FindClass("android/net/Uri");
    if (!uri_class || ClearPendingException(env)) break;
    jmethodID parse =
        env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    if (!parse || ClearPendingException(env)) break;

    const std::string uri_str(uri);
    jstring juri = env->NewStringUTF(uri_str.c_str());
    if (!juri || ClearPendingException(env)) break;

    jobject uri_obj = env->CallStaticObjectMethod(uri_class, parse, juri);
    if (ClearPendingException(env) || !uri_obj) break;

    // context.getContentResolver()
    jclass context_class = env->GetObjectClass(context);
    if (!context_class || ClearPendingException(env)) break;
    jmethodID get_resolver = env->GetMethodID(context_class, "getContentResolver",
                                              "()Landroid/content/ContentResolver;");
    if (!get_resolver || ClearPendingException(env)) break;
    jobject resolver = env->CallObjectMethod(context, get_resolver);
    if (ClearPendingException(env) || !resolver) break;

    // resolver.openFileDescriptor(uri, mode)
    jclass resolver_class = env->GetObjectClass(resolver);
    if (!resolver_class || ClearPendingException(env)) break;
    jmethodID open_fd = env->GetMethodID(
        resolver_class, "openFileDescriptor",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");
    if (!open_fd || ClearPendingException(env)) break;

    jstring jmode = env->NewStringUTF(NormalizeMode(mode));
    if (!jmode || ClearPendingException(env)) break;

    jobject pfd = env->CallObjectMethod(resolver, open_fd, uri_obj, jmode);
    // A missing or permission-denied URI throws FileNotFoundException/
    // SecurityException; both surface here as a cleared exception and null.
    if (ClearPendingException(env) || !pfd) break;

    // pfd.detachFd() -- transfers ownership of the fd to native code, so the
    // ParcelFileDescriptor being garbage collected will not close it.
    jclass pfd_class = env->GetObjectClass(pfd);
    if (!pfd_class || ClearPendingException(env)) break;
    jmethodID detach_fd = env->GetMethodID(pfd_class, "detachFd", "()I");
    if (!detach_fd || ClearPendingException(env)) break;

    const jint fd = env->CallIntMethod(pfd, detach_fd);
    if (ClearPendingException(env)) break;

    result_fd = static_cast<int>(fd);
  } while (false);

  env->PopLocalFrame(nullptr);
  return result_fd;
}

}  // namespace filesystem
}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
