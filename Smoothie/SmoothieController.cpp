#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include <pluginterfaces/vst/ivstmidicontrollers.h>

#include "Smoothie.h"
#include "SmoothieController.h"
#include <string>
#include <pluginterfaces/base/ustring.h>

SmoothnessParam::SmoothnessParam(void)
{
	precision = 2;
	info.defaultNormalizedValue = 0.5;
	maxPlain = std::numeric_limits<double>::infinity();
	LOG("SmoothnessParam default constructor called and exited.\n");
}

SmoothnessParam::~SmoothnessParam(void)
{
	LOG("SmoothnessParam destructor called and exited.\n");
}

SmoothnessParam::SmoothnessParam(const TChar* title, ParamID tag, UnitID unit_id) :
	RangeParameter(title, tag, nullptr, 0., std::numeric_limits<double>::infinity(), secs_per_half_slowness, 0, ParameterInfo::kCanAutomate, unit_id)
{
	precision = 2;
	info.defaultNormalizedValue = 0.5;
	LOG("SmoothnessParam constructor called and exited.\n");
}

ParamValue SmoothnessParam::toPlain(ParamValue normValue) const
{
	if (normValue <= 0.0)
		return 0.0;
	else if (normValue >= 1.0)
		return std::numeric_limits<double>::infinity();
	else
		return normValue * secs_per_half_slowness / (1.0 - normValue);
}

ParamValue SmoothnessParam::toNormalized(ParamValue plainValue) const
{
	if (plainValue <= 0)
		return 0.0;
	else
		return plainValue / (plainValue + secs_per_half_slowness);
}

void SmoothnessParam::toString(ParamValue normValue, String128 string) const
{
	if (normValue <= 0.0)
	{
		string[0] = '0';
		string[1] = 0;
	}
	else if (normValue >= toNormalized(24.0 * 60.0 * 60.0))
	{
		string[0] = u'\u221E';
		string[1] = 0;
	}
	else
	{
		ParamValue plainValue = toPlain(normValue);
		ParamValue printedValue = plainValue;
		TChar suffix[2] = {};
		if (plainValue >= 60.0 * 60.0)
		{
			suffix[0] = u'h';
			printedValue /= 60.0 * 60.0;
		}
		else if (plainValue >= 60.0)
		{
			suffix[0] = u'm';
			printedValue /= 60.0;
		}
		else
			suffix[0] = u's';

		UString wrapper(string, str16BufferSize(String128));
		if (wrapper.printFloat(printedValue, precision))
		{
			wrapper.append(suffix, 2);
		}
		else
			string[0] = 0;
	}
}

bool SmoothnessParam::fromString(const TChar* string, ParamValue& normValue) const
{
	ParamValue plainValue;

	if ((string[0] == u'\u221E') && (string[1] == 0))
	{
		normValue = 1.0;
		return true;
	}
	else if (RangeParameter::fromString(string, plainValue))
	{
		const TChar* suffix = string;
		while (*suffix) ++suffix;
		if (suffix > string)
		{
			switch (*(--suffix))
			{
			case u'h':
				plainValue *= 60.0 * 60.0;
				break;
			case u'm':
				plainValue *= 60.0;
				break;
			}
		}
		normValue = toNormalized(plainValue);
		return true;
	}
	else
		return false;
}

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
	QUERY_INTERFACE(iid, obj, IMidiMapping::iid, IMidiMapping)
	return EditControllerEx1::queryInterface(iid, obj);
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
	tresult result = EditControllerEx1::initialize(context);

	if (result != kResultOk)
	{
		LOG("SmoothieController::initialize exited prematurely with code %d.\n", result);
		return result;
	}

	char16_t unit_name[32] = STR16("Smoothed");
	char16_t in_name[32] = STR16("InParam");
	char16_t out_name[32] = STR16("OutParam");
	char16_t s_name[32] = STR16("Slowness");
	char16_t* unit_index = unit_name + std::char_traits<char16_t>::length(unit_name);
	char16_t* in_index = in_name + std::char_traits<char16_t>::length(in_name);
	char16_t* out_index = out_name + std::char_traits<char16_t>::length(out_name);
	char16_t* s_index = s_name + std::char_traits<char16_t>::length(s_name);

	for (int32 i = 0; i < num_smoothed_params; ++i)
	{
		uint32_to_str16(unit_index, i + 1);
		uint32_to_str16(in_index, i + 1);
		uint32_to_str16(out_index, i + 1);
		uint32_to_str16(s_index, i + 1);
		addUnit(new Unit(unit_name, i + 1));
		parameters.addParameter(in_name, nullptr, 0, 0., ParameterInfo::kCanAutomate, i * NumParamOffsets + InParamOffset, i + 1);
		parameters.addParameter(out_name, nullptr, 0, 0., ParameterInfo::kCanAutomate, i * NumParamOffsets + OutParamOffset, i + 1);
		parameters.addParameter(new SmoothnessParam(s_name, i * NumParamOffsets + SlownessOffset, i + 1));
	}

	LOG("SmoothieController::initialize exited normally with code %d.\n", result);
	return result;
}

tresult PLUGIN_API SmoothieController::terminate()
{
	LOG("SmoothieController::terminate called.\n");
	tresult result = EditControllerEx1::terminate();
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

tresult PLUGIN_API SmoothieController::getMidiControllerAssignment(int32 busIndex, int16 midiChannel, CtrlNumber midiControllerNumber, ParamID& tag)
{
	LOG("SmoothieController::getMidiControllerAssignment called.\n");
	if (busIndex == 0 && (CtrlNumber)cc <= midiControllerNumber && (ParamID)midiControllerNumber < (ParamID)cc + num_smoothed_params && midiChannel == 0)
	{
		tag = ((ParamID)midiControllerNumber - (ParamID)cc) * NumParamOffsets + InParamOffset;
		LOG("SmoothieController::getMidiControllerAssignment exited normally.\n");
		return kResultTrue;
	}
	LOG("SmoothieController:getMidiControllerAssignment exited with failure.\n");
	return kResultFalse;
}