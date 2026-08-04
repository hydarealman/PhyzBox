#pragma once

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_phyzbox_module(ModuleInitializationLevel level);
void uninitialize_phyzbox_module(ModuleInitializationLevel level);

extern "C" {
GDExtensionBool GDE_EXPORT phyzbox_library_init(
    GDExtensionInterfaceGetProcAddress getProcAddress,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization);
}
