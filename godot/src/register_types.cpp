#include "register_types.hpp"
#include "phyz_simulation.hpp"

#include <godot_cpp/godot.hpp>

void initialize_phyzbox_module(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(PhyzSimulation);
    }
}

void uninitialize_phyzbox_module(ModuleInitializationLevel) {}

extern "C" GDExtensionBool GDE_EXPORT phyzbox_library_init(
    GDExtensionInterfaceGetProcAddress getProcAddress,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject initObject(getProcAddress, library, initialization);
    initObject.register_initializer(initialize_phyzbox_module);
    initObject.register_terminator(uninitialize_phyzbox_module);
    initObject.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return initObject.init();
}
