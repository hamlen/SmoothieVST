#include "public.sdk/source/vst/vstaudioprocessoralgo.h"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include "Smoothie.h"
#include "SmoothieController.h"

Smoothie::Smoothie(void)
{
	LOG("Smoothie constructor called.\n");
	setControllerClass(FUID(SmoothieControllerUID));
	processSetup.maxSamplesPerBlock = 8192;
	LOG("Smoothie constructor exited.\n");
}

Smoothie::~Smoothie(void)
{
	LOG("Smoothie destructor called and exited.\n");
}

tresult PLUGIN_API Smoothie::initialize(FUnknown* context)
{
	LOG("Smoothie::initialize called.\n");
	tresult result = AudioEffect::initialize(context);

	if (result != kResultOk)
	{
		LOG("Smoothie::initialize failed with code %d.\n", result);
		return result;
	}

	addEventInput(STR16("Event In"));
	addEventOutput(STR16("Event Out"));

	LOG("Smoothie::initialize exited normally.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::terminate()
{
	LOG("Smoothie::terminate called.\n");
	tresult result = AudioEffect::terminate();
	LOG("Smoothie::terminate exited with code %d.\n", result);
	return result;
}

tresult PLUGIN_API Smoothie::setActive(TBool state)
{
	LOG("Smoothie::setActive called.\n");
	tresult result = AudioEffect::setActive(state);
	LOG("Smoothie::setActive exited with code %d.\n", result);
	return result;
}

tresult PLUGIN_API Smoothie::setIoMode(IoMode mode)
{
	LOG("Smoothie::setIoMode called and exited.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::setProcessing(TBool state)
{
	LOG("Smoothie::setProcessing called and exited.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::setState(IBStream* state)
{
	LOG("Smoothie::setState called.\n");

	IBStreamer streamer(state, kLittleEndian);
	for (int32 i = 0; i < num_smoothed_params; ++i)
	{
		double vals[NumParamOffsets];
		if (!streamer.readDoubleArray(vals, NumParamOffsets))
		{
			LOG("Smoothie::setState stopped early with %dx3 records read.\n", i);
			return kResultOk;
		}
		values[i].in = vals[0];
		values[i].out = vals[1];
		values[i].slowness = vals[2];
	}

	LOG("Smoothie::setState exited successfully.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::getState(IBStream* state)
{
	LOG("Smoothie::getState called.\n");

	IBStreamer streamer(state, kLittleEndian);
	for (int32 i = 0; i < num_smoothed_params; ++i)
	{
		if (!streamer.writeDoubleArray((ParamValue*)&values[i], NumParamOffsets))
		{
			LOG("Smoothie::getState failed due to streamer error.\n");
			return kResultFalse;
		}
	}

	LOG("Smoothie::getState exited successfully.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts)
{
	// We just say "ok" to all speaker arrangements because this plug-in doesn't process any audio,
	// but some hosts refuse to load any plug-in for which they can't find a speaker arrangement.
	LOG("Smoothie::setBusArrangements called and exited.\n");
	return kResultOk;
}

tresult PLUGIN_API Smoothie::setupProcessing(ProcessSetup& newSetup)
{
	LOG("Smoothie::setupProcessing called.\n");
	processContextRequirements.flags = 0;
	tresult result = AudioEffect::setupProcessing(newSetup);
	LOG("Smoothie::setupProcessing exited with code %d.\n", result);
	return result;
}

tresult PLUGIN_API Smoothie::canProcessSampleSize(int32 symbolicSampleSize)
{
	LOG("Smoothie::canProcessSampleSize called with arg %d and exited.\n", symbolicSampleSize);
	return (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Smoothie::getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo)
{
	LOG("Smoothie::getRoutingInfo called.\n");
	if (inInfo.mediaType == kEvent && inInfo.busIndex == 0)
	{
		outInfo = inInfo;
		LOG("Smoothie::getRoutingInfo exited with success.\n");
		return kResultOk;
	}
	else
	{
		LOG("Smoothie::getRoutingInfo exited with failure.\n");
		return kResultFalse;
	}
}

constexpr double small_double = 0.00001;
#define CONSTRAIN(var) if ((var) < 0.) (var) = 0.; else if ((var) > 1.) (var) = 1.

void Smoothie::sendEvents(IEventList* send, int32 firstSampleOffset, int32 finalSampleOffset, int8 CCnum, int8 firstCCval, int8 finalCCval)
{
	if ((finalSampleOffset <= firstSampleOffset) || (finalCCval == firstCCval))
		return;

	const int32 xrange = finalSampleOffset - firstSampleOffset;
	const int32 yrange = finalCCval - firstCCval;
	const double slope = (double)yrange / (double)xrange;

	Event e = {};
	e.type = e.kLegacyMIDICCOutEvent;
	e.midiCCOut.controlNumber = CCnum;

	if (xrange <= yrange)
	{
		for (int32 i = 1; i <= xrange; ++i)
		{
			int8 y = firstCCval + std::round((double)i * slope);
			if (y < 0) y = 0; else if (y > 127) y = 127;
			e.sampleOffset = firstSampleOffset + i;
			e.midiCCOut.value = y;
			if (send)
				send->addEvent(e);
		}
	}
	else
	{
		for (int32 j = 1; j <= yrange; ++j)
		{
			int8 x = firstSampleOffset + std::round((double)j / slope);
			if (x >= finalSampleOffset) x = finalSampleOffset;
			e.sampleOffset = x;
			e.midiCCOut.value = firstCCval + j;
			if (send)
				send->addEvent(e);
		}
	}
}

tresult PLUGIN_API Smoothie::process(ProcessData& data)
{
	// We shouldn't be asked for any audio, but process it anyway (emit silence) to tolerate uncompliant hosts.
	for (int32 i = 0; i < data.numOutputs; ++i)
	{
		for (int32 j = 0; j < data.outputs[i].numChannels; ++j)
		{
			bool is32bit = (data.symbolicSampleSize == kSample32);
			if (is32bit || (data.symbolicSampleSize == kSample64))
			{
				void* channelbuffer = is32bit ? (void*)data.outputs[i].channelBuffers32[j] : (void*)data.outputs[i].channelBuffers64[j];
				size_t datasize = is32bit ? sizeof(*data.outputs[i].channelBuffers32[j]) : sizeof(*data.outputs[i].channelBuffers64[j]);
				if (channelbuffer)
					memset(channelbuffer, 0, data.numSamples * datasize);
			}
		}
		data.outputs[i].silenceFlags = (1ULL << data.outputs[i].numChannels) - 1;
	}

	// Incoming CC events jump the InParam to the specified value.
	if (data.inputEvents)
	{
		int32 count = data.inputEvents->getEventCount();
		for (int32 i = 0; i < count; ++i)
		{
			Event e;
			if (data.inputEvents->getEvent(i, e) != kResultOk)
				return kResultFalse;
			if (e.type == e.kLegacyMIDICCOutEvent && cc <= e.midiCCOut.controlNumber && e.midiCCOut.controlNumber < cc + num_smoothed_params)
			{
				ParamValue o = ((ParamValue)e.midiCCOut.value / 127.);
				CONSTRAIN(o);
				values[e.midiCCOut.controlNumber - cc].in = o;
			}
		}
	}

	IParameterChanges* params_in = data.inputParameterChanges;
	IParameterChanges* params_out = data.outputParameterChanges;

	// Process incoming parameter automations:
	// (1) Find and save event queues for changes to InParam, for later sample-accurate processing.
	// (2) Changes to OutParam immediately jump the OutParam to the requested value.
	// (3) Changes to Slowness immediate jump the Slowness to the requested value.
	IParamValueQueue* in_queue[num_smoothed_params] = {};
	bool slowness_changed[num_smoothed_params] = {};
	if (params_in)
	{
		int32 numParamsChanged = params_in->getParameterCount();

		for (int32 i = 0; i < numParamsChanged; ++i)
		{
			IParamValueQueue* q = params_in->getParameterData(i);
			ParamID id = q->getParameterId();
			int32 count;
			if (id < num_smoothed_params * NumParamOffsets)
			{
				ParamID setid = id / NumParamOffsets;
				switch (id % NumParamOffsets)
				{
				case InParamOffset:
					in_queue[setid] = q;
					break;
				case OutParamOffset:
					count = q->getPointCount();
					for (int32 j = 0; j < count; ++j)
					{
						int32 dummy;
						if (q->getPoint(j, dummy, values[setid].out) != kResultOk)
							return kResultFalse;
					}
					break;
				case SlownessOffset:
					count = q->getPointCount();
					for (int32 j = 0; j < count; ++j)
					{
						int32 dummy;
						if (q->getPoint(j, dummy, values[setid].slowness) != kResultOk)
							return kResultFalse;
						slowness_changed[setid] = true;
					}
					break;
				}
			}
		}
	}

	for (int32 i = 0; i < num_smoothed_params; ++i)
	{
		CONSTRAIN(values[i].out);
		CONSTRAIN(values[i].slowness);
	}

	// Find output event queues for automated parameters, creating them if necessary.
	IParamValueQueue* out_queue[num_smoothed_params] = {};
	if (params_out)
	{
		IParamValueQueue* max_slope_queue[num_smoothed_params] = {};
		int32 dummy;

		int32 count = params_out->getParameterCount();
		for (int32 i = 0; i < count; ++i)
		{
			IParamValueQueue* q = params_out->getParameterData(i);
			ParamID id = q->getParameterId();
			if (id < num_smoothed_params * NumParamOffsets)
			{
				ParamID setid = id / NumParamOffsets;
				switch (id % NumParamOffsets)
				{
				case OutParamOffset:
					out_queue[setid] = q;
					break;
				case SlownessOffset:
					max_slope_queue[setid] = q;
					break;
				}
			}
		}

		for (int32 setid = 0; setid < num_smoothed_params; ++setid)
		{
			if (!out_queue[setid])
				out_queue[setid] = params_out->addParameterData(setid * NumParamOffsets + OutParamOffset, dummy);

			if (slowness_changed[setid])
			{
				if (!max_slope_queue[setid])
					max_slope_queue[setid] = params_out->addParameterData(setid * NumParamOffsets + SlownessOffset, dummy);
				if (max_slope_queue[setid]->addPoint(0, values[setid].slowness, dummy) != kResultOk)
					return kResultFalse;
			}
		}
	}

	if (data.numSamples <= 0)
		return kResultTrue;

	if (!data.processContext || data.processContext->sampleRate <= 0.)
	{
		LOG("Smoothie::process aborted due to bad sample rate provided by host.\n");
		return kResultFalse;
	}

	/* Begin sample-accurate processing of InParam -> OutParam smoothing:
	 * 
	 * slowness=0 -> changes to InParam are instantly reflected to OutParam
	 * slowness=.5 -> changes to InParam cause OutParam to move at a rate of secs_per_half_slowness toward InParam
	 * slowness=1 -> changes to InParam cause OutParam to react infinitely slowly (OutParam never changes)
	 *
	 * In general, to make OutParam take n seconds to move from 0 to 1, set slowness to:
	 *   slowness = n / (n + h)
	 * where h = secs_per_half_slowness (default=2)
	 */
	
	for (int32 sid = 0; sid < num_smoothed_params; ++sid)
	{
		double slowness = values[sid].slowness;
		double max_slope =
			(slowness <= 0.) ? 1. : ((1. - slowness) / slowness / secs_per_half_slowness / data.processContext->sampleRate);
		int32 lastSampleOffset = -1;
		int32 numPoints = in_queue[sid] ? in_queue[sid]->getPointCount() : 0;

		for (int32 i = 0; i <= numPoints; ++i)
		{
			int8 lastCC = std::round(127. * values[sid].out);
			int32 sampleOffset = data.numSamples - 1;
			ParamValue value = values[sid].in;
			if (i < numPoints)
			{
				if (in_queue[sid]->getPoint(i, sampleOffset, value) != kResultOk)
					return kResultFalse;
				CONSTRAIN(value);
			}

			if (sampleOffset > lastSampleOffset)
			{
				int32 dummy;
				const double in_slope = (value - values[sid].in) / (double)(sampleOffset - lastSampleOffset);
				double out_slope;
				double param_diff = values[sid].in - values[sid].out;
				if (param_diff < 0.)
					param_diff = -param_diff;

				// If the OutParam can catch the InParam's automation curve (without exceeding speed max_slope) before
				// the next point in InParam's automation curve, output an extra automation curve point for OutParam
				// at the intersection point of the two curves.
				if (param_diff > small_double)
				{
					out_slope = (values[sid].in > values[sid].out) ? max_slope : -max_slope;
					const int32 intersection = (in_slope == out_slope) ? sampleOffset
						: (lastSampleOffset + (int32)std::round(((double)(values[sid].in - values[sid].out) / (out_slope - in_slope))));
					if (lastSampleOffset < intersection && intersection < sampleOffset)
					{
						double y = values[sid].in + in_slope * (double)(intersection - lastSampleOffset);
						CONSTRAIN(y);

						if (out_queue[sid])
							out_queue[sid]->addPoint(intersection, y, dummy);

						int8 cc_value = std::round(127. * y);
						if (cc_value < 0) cc_value = 0; else if (cc_value > 127) cc_value = 127;
						sendEvents(data.outputEvents, lastSampleOffset, intersection, cc + sid, lastCC, cc_value);
						lastCC = cc_value;

						values[sid].out = values[sid].in = y;
						lastSampleOffset = intersection;
						param_diff = 0.;
					}
					else
					{
						values[sid].out += out_slope * (double)(sampleOffset - lastSampleOffset);
					}
				}

				// If OutParam can catch InParam at InParam's next automation point (without exceeding speed max_slope),
				// then move it there.  Otherwise move it at its maximum allowed speed toward InParam.
				if (param_diff <= small_double)
				{
					if (in_slope < -max_slope)
					{
						out_slope = -max_slope;
						values[sid].out = values[sid].in + out_slope * (double)(sampleOffset - lastSampleOffset);
					}
					else if (in_slope <= max_slope)
					{
						out_slope = in_slope;
						values[sid].out = value;
					}
					else
					{
						out_slope = max_slope;
						values[sid].out = values[sid].in + out_slope * (double)(sampleOffset - lastSampleOffset);
					}
				}

				CONSTRAIN(values[sid].out);

				// Output the computed automation curve point for OutParam, and send a smooth ramp of midi CC
				// values corresponding to its path to that point.
				if ((i < numPoints) || (param_diff > small_double))
				{
					if (out_queue[sid])
						out_queue[sid]->addPoint(sampleOffset, values[sid].out, dummy);

					int8 cc_value = std::round(127. * values[sid].out);
					if (cc_value < 0) cc_value = 0; else if (cc_value > 127) cc_value = 127;
					sendEvents(data.outputEvents, lastSampleOffset, sampleOffset, cc + sid, lastCC, cc_value);
					lastCC = cc_value;
				}

				lastSampleOffset = sampleOffset;
			}
			values[sid].in = value;
		}
	}

	return kResultOk;
}
