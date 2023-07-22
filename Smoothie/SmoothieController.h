#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

// Plugin controller GUID - must be unique
static const FUID SmoothieControllerUID(0xbe1df3c4, 0x903a464c, 0xbc64cea7, 0xa2059f50);

class SmoothnessParam : public RangeParameter
{
public:
	SmoothnessParam(void);
	SmoothnessParam(const TChar* title, ParamID tag, UnitID unit_id);

	void setMin(ParamValue value) {};
	void setMax(ParamValue value) {};

	ParamValue toPlain(ParamValue normValue) const SMTG_OVERRIDE;
	ParamValue toNormalized(ParamValue plainValue) const SMTG_OVERRIDE;
	void toString(ParamValue normValue, String128 string) const SMTG_OVERRIDE;
	bool fromString(const TChar* string, ParamValue& normValue) const SMTG_OVERRIDE;

	~SmoothnessParam(void);
};

class SmoothieController : public EditControllerEx1, public IMidiMapping
{
public:
	SmoothieController(void);

	static FUnknown* createInstance(void* context)
	{
		return (IEditController*) new SmoothieController;
	}

	DELEGATE_REFCOUNT(EditControllerEx1)
	tresult PLUGIN_API queryInterface(const char* iid, void** obj) SMTG_OVERRIDE;

	tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate() SMTG_OVERRIDE;
	tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel, CtrlNumber midiControllerNumber, ParamID& id) SMTG_OVERRIDE;

	~SmoothieController(void);
};

