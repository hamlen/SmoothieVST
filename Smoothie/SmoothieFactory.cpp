#include "Smoothie.h"
#include "SmoothieController.h"

#include "public.sdk/source/main/pluginfactory.h"

#define PluginCategory "Fx"
#define PluginName "Smoothie"

#define PLUGINVERSION "1.0.0"

bool InitModule()
{
	LOG("InitModule called and exited.\n");
	return true;
}

bool DeinitModule()
{
	LOG("DeinitModule called and exited.\n");
	return true;
}

BEGIN_FACTORY_DEF("Kevin Hamlen",
	"no website",
	"no contact")

	LOG("GetPluginFactory called.\n");

	DEF_CLASS2(INLINE_UID_FROM_FUID(SmoothieProcessorUID),
		PClassInfo::kManyInstances,
		kVstAudioEffectClass,
		PluginName,
		Vst::kDistributable,
		PluginCategory,
		PLUGINVERSION,
		kVstVersionString,
		Smoothie::createInstance)

	DEF_CLASS2(INLINE_UID_FROM_FUID(SmoothieControllerUID),
		PClassInfo::kManyInstances,
		kVstComponentControllerClass,
		PluginName "Controller",
		0,  // unused
		"", // unused
		PLUGINVERSION,
		kVstVersionString,
		SmoothieController::createInstance)

END_FACTORY
