#pragma once

#undef LOGGING

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "base/source/fstring.h"
#include "pluginterfaces/base/funknown.h"
#include <pluginterfaces/vst/ivstparameterchanges.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

constexpr Steinberg::Vst::ParamID num_smoothed_params = 8;
constexpr double secs_per_half_slowness = 2;  // seconds to go from 0 to 1 when slowness = .5

// Parameter enumeration
enum SmoothieParamOffsets : Steinberg::Vst::ParamID
{
	InParamOffset = 0,
	OutParamOffset = 1,
	SlownessOffset = 2,
	NumParamOffsets = 3,
};

constexpr uint8 cc = 90;
#if cc + num_smoothed_params > 127
#  error "Not enough CC slots for smoothed params."
#endif

// Plugin processor GUID - must be unique
static const FUID SmoothieProcessorUID(0xbe1df3c4, 0x903a464c, 0xbc64cea7, 0xa2059f4f);

typedef struct param_set {
	ParamValue in = 0;
	ParamValue out = 0;
	ParamValue slowness = .5;
} ParamSet;

class Smoothie : public AudioEffect
{
public:
	Smoothie(void);

	static FUnknown* createInstance(void* context)
	{
		return (IAudioProcessor*) new Smoothie();
	}

	tresult PLUGIN_API initialize(FUnknown* context);
	tresult PLUGIN_API terminate();
	tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup);
	tresult PLUGIN_API setActive(TBool state);
	tresult PLUGIN_API setProcessing(TBool state);
	tresult PLUGIN_API process(ProcessData& data);
	tresult PLUGIN_API getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo);
	tresult PLUGIN_API setIoMode(IoMode mode);
	tresult PLUGIN_API setState(IBStream* state);
	tresult PLUGIN_API getState(IBStream* state);
	tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize);
	~Smoothie(void);

protected:
	ParamSet values[num_smoothed_params];
	bool initial_points_sent = false;
	void addOutPoint(ProcessData& data, IParamValueQueue*& pqueue, int32 param_set, int32 firstSampleOffset, int32 finalSampleOffset, int8& prevCCval, double finalval);
};

#ifdef LOGGING
	void log(const char* format, ...);
#	define LOG(format, ...) log((format), __VA_ARGS__)
#else
#	define LOG(format, ...) 0
#endif
