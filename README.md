# Coco-Deferred-Renderer

This repository contains a deferred renderer implemented in C++ an OpenGL as well as a multi threaded asset builder. The program is built for visual studio with the .sln files stored in build_win32.

Build: To build the program, open a command promt and navigate to the build directory. Run build.bat and the project should be built.

Program Entry Point: The main entry point for the deferred renderer is found at win32_coco.cpp while the deferred renderer's main entry point is found in the function GameUpdateAndRender in coco.cpp. The main entry point for the asset builder is found in coco_asset_builder.cpp (both programs get compiled with the build.bat call).

Controls: The controls are WASD for camera movement and the mouse pointer rotates the camera around. You can also set which scene you want to render in the #if statements found in GameUpdateAndRender.
