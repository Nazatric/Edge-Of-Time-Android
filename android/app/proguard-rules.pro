# The app keeps all of its own classes; the SDL activity classes are invoked
# reflectively by SDL's native JNI glue (org.libsdl.app.*), so keep them too.
-keep class org.libsdl.app.** { *; }
-keep class com.nazatric.edgeoftime.** { *; }

# JNI-called native methods are resolved by name; keep the NativeBridge surface.
-keepclasseswithmembernames class * {
    native <methods>;
}
