# Vendored SDL3 Android Java glue

The Java files in this directory (`SDLActivity.java`, `SDLSurface.java`,
`SDLControllerManager.java`, `SDL.java`, `HIDDevice*.java`, `SDLAudioManager.java`,
`SDLInputConnection.java`, `SDLDummyEdit.java`) are copied **verbatim** from the
SDL3 source tree vendored by the ReXGlue SDK:

    thirdparty/rexglue-sdk/thirdparty/sdl3/android-project/app/src/main/java/org/libsdl/app/

They are the standard SDL3 Android activity glue that every SDL3 Android app
uses. They are required because SDL's native JNI glue (`SDL_android.c`,
compiled into `librexruntime.so`) registers its native methods against the
class names `org.libsdl.app.SDLActivity` / `org.libsdl.app.SDLSurface`, and
`SDLActivity.nativeRunMain` resolves and runs the app's `SDL_main`.

License: **zlib** (see `thirdparty/rexglue-sdk/thirdparty/sdl3/LICENSE.txt`,
Copyright (C) 1997-2026 Sam Lantinga). SDL3's zlib license permits verbatim
redistribution; no modifications were made to these files. If the vendored
SDL3 is ever updated, re-copy these files from the matching SDL3 revision and
keep them unmodified.
