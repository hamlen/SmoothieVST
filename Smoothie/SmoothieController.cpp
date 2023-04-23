#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include <pluginterfaces/vst/ivstmidicontrollers.h>

#include "Smoothie.h"
#include "SmoothieController.h"
#include <string>

SmoothieController::SmoothieController(void)
{
	LOG("SmoothieController constructor called and exited.\n");
}

SmoothieController::~SmoothieController(void)
{
	LOG("SmoothieController destructor called and exited.\n");
}

tresult PLUGIN_API SmoothieController::queryInterface(const char* iid, void** obj)
{
	return EditController::queryInterface(iid, obj);
}

static void uint32_to_str16(TChar* p, uint32 n)
{
	if (n == 0)
	{
		*p++ = u'0';
		*p = 0;
	}
	else
	{
		uint32 width = 0;
		for (uint32 i = n; i; i /= 10)
			++width;

		*(p + width) = 0;
		for (uint32 i = n; width > 0; i /= 10)
			*(p + --width) = STR16("0123456789")[i % 10];
	}
}

tresult PLUGIN_API SmoothieController::initialize(FUnknown* context)
{
	LOG("SmoothieController::initialize called.\n");
	tresult result = EditController::initialize(context);

	if (result != kResultOk)
	{
		LOG("SmoothieController::initialize exited prematurely with code %d.\n", result);
		return result;
	}

	char16_t in_name[32] = STR16("InParam");
	char16_t out_name[32] = STR16("OutParam");
	char16_t s_name[32] = STR16("Slowness");
	char16_t* in_index = in_name + std::char_traits<char16_t>::length(in_name);
	char16_t* out_index = out_name + std::char_traits<char16_t>::length(out_name);
	char16_t* s_index = s_name + std::char_traits<char16_t>::length(s_name);

	for (int32 i = 0; i < num_smoothed_params; ++i)
	{
		uint32_to_str16(in_index, i + 1);
		uint32_to_str16(out_index, i + 1);
		uint32_to_str16(s_index, i + 1);
		parameters.addParameter(in_name, nullptr, 0, 0., ParameterInfo::kCanAutomate, i * NumParamOffsets + InParamOffset);
		parameters.addParameter(out_name, nullptr, 0, 0., ParameterInfo::kCanAutomate, i * NumParamOffsets + OutParamOffset);
		parameters.addParameter(s_name, nullptr, 0, .5, ParameterInfo::kCanAutomate, i * NumParamOffsets + SlownessOffset);
	}

	LOG("SmoothieController::initialize exited normally with code %d.\n", result);
	return result;
}

tresult PLUGIN_API SmoothieController::terminate()
{
	LOG("SmoothieController::terminate called.\n");
	tresult result = EditController::terminate();
	LOG("SmoothieController::terminate exited with code %d.\n", result);
	return result;
}

tresult PLUGIN_API SmoothieController::setComponentState(IBStream* state)
{
	LOG("SmoothieController::setComponentState called.\n");
	if (!state)
	{
		LOG("SmoothieController::setComponentState failed because no state argument provided.\n");
		return kResultFalse;
	}

	IBStreamer streamer(state, kLittleEndian);
	for (uint32 i = 0; i < num_smoothed_params; ++i)
	{
		ParamValue vals[NumParamOffsets];
		if (!streamer.readDoubleArray(vals, NumParamOffsets))
		{
			LOG("SmoothieController::setComponentState stopped early with %dx3 records read.\n", i);
			return kResultOk;
		}
		setParamNormalized(i * NumParamOffsets + InParamOffset, vals[0]);
		setParamNormalized(i * NumParamOffsets + OutParamOffset, vals[1]);
		setParamNormalized(i * NumParamOffsets + SlownessOffset, vals[2]);
	}

	LOG("SmoothieController::setComponentState exited normally.\n");
	return kResultOk;
}
