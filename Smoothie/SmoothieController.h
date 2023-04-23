#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

// Plugin controller GUID - must be unique
static const FUID SmoothieControllerUID(0xbe1df3c4, 0x903a464c, 0xbc64cea7, 0xa2059f50);

class SmoothieController : public EditController
{
public:
	SmoothieController(void);

	static FUnknown* createInstance(void* context)
	{
		return (IEditController*) new SmoothieController;
	}

	DELEGATE_REFCOUNT(EditController)
	tresult PLUGIN_API queryInterface(const char* iid, void** obj) SMTG_OVERRIDE;

	tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate() SMTG_OVERRIDE;

	tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE;

	// Uncomment to add a GUI
	// IPlugView * PLUGIN_API createView (const char * name);

	// Uncomment to override default EditController behavior
	// tresult PLUGIN_API setState(IBStream* state);
	// tresult PLUGIN_API getState(IBStream* state);
	// tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value);
	// tresult PLUGIN_API getParamStringByValue(ParamID tag, ParamValue valueNormalized, String128 string);
	// tresult PLUGIN_API getParamValueByString(ParamID tag, TChar* string, ParamValue& valueNormalized);

	~SmoothieController(void);
};

