// Force the linker to pull in all script registration objects.
// Each script .cpp file contains a static REGISTER_SCRIPT struct whose
// constructor registers the script at startup. By referencing each header,
// we ensure those translation units are part of the link.
#include "TestScript.hpp"
#include "WASDMovement.hpp"

// Dummy function referenced from main so the linker can't discard this TU.
namespace ST { void ST_ScriptInit_force_link() {} }
