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
	processSetup.maxSamplesPerBlock = INT32_MAX;
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
	initial_points_sent = false;
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
static inline bool roughly_equal(ParamValue x, ParamValue y)
{
	ParamValue diff = x - y;
	return (-small_double <= diff) && (diff <= small_double);
}

#define CONSTRAIN(var) if ((var) < 0.) (var) = 0.; else if ((var) > 1.) (var) = 1.

static void output_initial_point(IParameterChanges* out_changes, ParamID id, ParamValue y)
{
	int32 dummy;
	int32 numParamsChanged = out_changes->getParameterCount();
	for (int32 i = 0; i < numParamsChanged; ++i)
	{
		IParamValueQueue* q = out_changes->getParameterData(i);
		if (q && q->getParameterId() == id)
		{
			int32 offset;
			ParamValue val;
			if (q->getPointCount() <= 0 || q->getPoint(0, offset, val) != kResultOk || offset > 0)
				q->addPoint(0, y, dummy);
			return;
		}
	}
	IParamValueQueue* q = out_changes->addParameterData(id, dummy);
	if (q)
		q->addPoint(0, y, dummy);
}

void Smoothie::addOutPoint(ProcessData& data, IParamValueQueue*& pqueue, int32 param_set, int32 firstSampleOffset, int32 finalSampleOffset, int8& prevCCval, double finalval)
{
	if (data.outputParameterChanges)
	{
		int32 dummy;
		if (!pqueue)
			pqueue = data.outputParameterChanges->addParameterData(param_set * NumParamOffsets + OutParamOffset, dummy);
		if (pqueue)
			pqueue->addPoint(finalSampleOffset, finalval, dummy);
	}
	values[param_set].out = finalval;

	const int8 firstCCval = prevCCval;
	int8 finalCCval = std::round(127. * finalval);
	if (finalCCval < 0) finalCCval = 0; else if (finalCCval > 127) finalCCval = 127;

	if (finalCCval != firstCCval && data.outputEvents)
	{
		Event e = {};
		e.type = e.kLegacyMIDICCOutEvent;
		e.midiCCOut.controlNumber = cc + param_set;

		if (finalSampleOffset <= firstSampleOffset)
		{
			e.sampleOffset = finalSampleOffset;
			e.midiCCOut.value = prevCCval = finalCCval;
			data.outputEvents->addEvent(e);
		}
		else
		{
			const int32 xrange = finalSampleOffset - firstSampleOffset;
			const int32 yrange = (int32)finalCCval - (int32)firstCCval;
			const double slope = (double)yrange / (double)xrange;

			if (slope >= 0.5 || slope <= -0.5)
			{
				for (int32 i = 1; i <= xrange; ++i)
				{
					int32 y = firstCCval + (int32)std::round((double)i * slope);
					if (y < 0) y = 0; else if (y > 127) y = 127;
					if (y != prevCCval)
					{
						e.sampleOffset = firstSampleOffset + i;
						e.midiCCOut.value = prevCCval = (int8)y;
						data.outputEvents->addEvent(e);
					}
				}
			}
			else
			{
				const int8 ysign = (firstCCval <= finalCCval) ? 1 : -1;
				for (int8 y = firstCCval; y != finalCCval; )
				{
					y += ysign;
					const int32 x = firstSampleOffset + (int32)std::round((double)(y - (int32)firstCCval) / slope);
					if (x > firstSampleOffset)
					{
						e.sampleOffset = (x >= finalSampleOffset) ? finalSampleOffset : x;
						e.midiCCOut.value = y;
						data.outputEvents->addEvent(e);
					}
				}
			}
		}
	}

	prevCCval = finalCCval;
}

static ParamValue interpolate(int32 x0, ParamValue y0, int32 x1, ParamValue y1, int32 x)
{
	if (x1 == x0)
	{
		if (y1 == y0)
			return y0;
		else if (x == x0)
			return y1;
		else if (y1 > y0)
			return (x > x1) ? 1. : 0.;
		else
			return (x > x1) ? 0. : 1.;
	}
	else
	{
		ParamValue y = (ParamValue)(x - x0) / (ParamValue)(x1 - x0) * (y1 - y0) + y0;
		CONSTRAIN(y);
		return y;
	}
}

tresult PLUGIN_API Smoothie::process(ProcessData& data)
{
	if (!data.processContext || data.processContext->sampleRate <= 0. || data.numSamples < 0)
	{
		LOG("Smoothie::process aborted due to bad sample rate provided by host.\n");
		return kResultFalse;
	}

	// We shouldn't be asked for any audio, but process it anyway (emit silence) to tolerate uncompliant hosts.
	const bool is32bit = (data.symbolicSampleSize == kSample32);
	const size_t buffersize = data.numSamples * ((data.symbolicSampleSize == kSample32) ? sizeof(Sample32) : sizeof(Sample64));
	if ((is32bit || (data.symbolicSampleSize == kSample64)) && (buffersize > 0))
	{
		for (int32 i = 0; i < data.numOutputs; ++i)
		{
			for (int32 j = 0; j < data.outputs[i].numChannels; ++j)
			{
				if (void* channelbuffer = is32bit ? (void*)data.outputs[i].channelBuffers32[j] : (void*)data.outputs[i].channelBuffers64[j])
					memset(channelbuffer, 0, buffersize);
			}
			data.outputs[i].silenceFlags = (1ULL << data.outputs[i].numChannels) - 1ULL;
		}
	}

	// Organize host-provided incoming parameter change queues into arrays.
	IParamValueQueue* in_queue[num_smoothed_params][NumParamOffsets] = {};
	if (data.inputParameterChanges)
	{
		int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
		for (int32 i = 0; i < numParamsChanged; ++i)
		{
			IParamValueQueue* q = data.inputParameterChanges->getParameterData(i);
			ParamID id = q->getParameterId();
			if (id < num_smoothed_params * NumParamOffsets)
				in_queue[id / NumParamOffsets][id % NumParamOffsets] = q;
		}
	}

	// If the host wants to flush parameters without processing, do so and exit.
	if (data.numSamples <= 0)
	{
		for (ParamID i = 0; i < num_smoothed_params; ++i)
			for (ParamID j = 0; j < NumParamOffsets; ++j)
				if (IParamValueQueue* q = in_queue[i][j])
				{
					const int32 n = q->getPointCount();
					if (n > 0)
					{
						ParamValue* y = (j == InParamOffset) ? &values[i].in : (j == OutParamOffset) ? &values[i].out : &values[i].slowness;
						int32 dummy;
						q->getPoint(n - 1, dummy, *y);
					}
				}
		return kResultOk;
	}

	// Organize host-provided outgoing parameter change queues into arrays.
	IParamValueQueue* out_queue[num_smoothed_params] = {};
	if (data.outputParameterChanges)
	{
		int32 numParamsChanged = data.outputParameterChanges->getParameterCount();
		for (int32 i = 0; i < numParamsChanged; ++i)
		{
			IParamValueQueue* q = data.outputParameterChanges->getParameterData(i);
			ParamID id = q->getParameterId();
			ParamID po = id % NumParamOffsets;
			if (id < num_smoothed_params * NumParamOffsets && po == OutParamOffset)
				out_queue[id / NumParamOffsets] = q;
		}
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

	for (ParamID param_set = 0; param_set < num_smoothed_params; ++param_set)
	{
		IParamValueQueue* const* const in_q = in_queue[param_set];
		const ParamValue saved_original_outval = values[param_set].out;

		// (in_x0,in_y0)--(in_x1,in_y1) is the last processed segment in InParam's automation curve,
		// and in_index is the index of the next point in its curve.
		int32 in_x0 = -1;
		ParamValue in_y0 = values[param_set].in;
		int32 in_x1 = -1;
		ParamValue in_y1 = in_y0;
		int32 in_index = 0;

		// (slowness_x,slowness) is the last processed point in Slowness's automation curve,
		// and slowness_index is the index of the next point in its curve.
		int32 slowness_x = -1;
		ParamValue slowness = values[param_set].slowness;
		int32 slowness_index = 0;

		// lastCC is the most recent CC value output for OutParam
		int8 lastCC = std::round(127. * values[param_set].out);

		// Count the number points in each parameter's incoming automation curve.
		int32 numPoints[NumParamOffsets] = {};
		for (ParamID i = 0; i < NumParamOffsets; ++i)
			if (in_q[i])
			{
				numPoints[i] = in_q[i]->getPointCount();
				if (numPoints[i] < 0) numPoints[i] = 0; // should never happen (host served invalid point count)
			}

		// (out_x0,out_y0) = the last OutParam automation curve point that was output.
		// (The point at offset -1 was implicitly output by the last call to process().)
		// Invariant: in_x0 <= out_x0
		int32 out_x0 = -1;
		ParamValue out_y0 = values[param_set].out;
		for (int32 out_index = 0; (uint32)out_index <= (uint32)numPoints[OutParamOffset]; ++out_index)
		{
			int32 out_x1 = data.numSamples - 1;
			ParamValue out_y1 = out_y0;
			if (out_index < numPoints[OutParamOffset])
			{
				in_q[OutParamOffset]->getPoint(out_index, out_x1, out_y1);
				if (out_x1 >= data.numSamples) out_x1 = data.numSamples - 1; // should never happen (host served invalid point queue)
				CONSTRAIN(out_y1);
			}
			if (out_x1 <= out_x0)
			{
				// should never happen (host served invalid point queue)
				out_y0 = out_y1;
				continue;
			}
			else if (!roughly_equal(out_y0, out_y1))
			{
				// The received curve for OutParam changed it over interval (out_x0, out_x1],
				// overriding any smoothing, so output that segment as-received.
				addOutPoint(data, out_queue[param_set], param_set, out_x0, out_x1, lastCC, out_y1);
				out_x0 = out_x1;
				out_y0 = out_y1;
				continue;
			}
			// Postcondition: in_x0 <= out_x0 < out_x1 < numSamples

			// The received curve for OutParam didn't change (much) over interval (out_x0, out_x1].
			// Merge all consecutive segments that don't change it (much) until we reach a segment
			// that does change it, or we reach the end of the sample buffer.
			while (out_index < numPoints[OutParamOffset])
			{
				int32 out_x2 = data.numSamples - 1;
				ParamValue out_y2 = out_y1;
				if (out_index + 1 < numPoints[OutParamOffset])
					in_q[OutParamOffset]->getPoint(out_index + 1, out_x2, out_y2);
				CONSTRAIN(out_y2);
				if (!roughly_equal(out_y1, out_y2))
					break;
				++out_index;
				if (out_x2 >= data.numSamples) out_x2 = data.numSamples - 1; // should never happen (host served invalid point queue)
				if (out_x1 < out_x2) out_x1 = out_x2; // should always happen (otherwise host served invalid point queue)
				out_y1 = out_y2;
			}
			// Postcondition: in_x0 <= out_x0 < out_x1 < numSamples

			// OutParam is unchanging over interval (out_x0, out_x1], and out_x1 is either the
			// start of a host-overridden segment or the end of the sample buffer (numSamples - 1).
			// Proceed to smoothly migrate OutParam toward InParam over interval (out_x0, out_x1]...

			while (out_x0 < out_x1)
			{
				// Find the first segment of InParam's automation curve that ends strictly after out_x0
				// Invariant: in_x0 <= out_x0 < out_x1 < numSamples
				while (in_x1 <= out_x0)
				{
					in_x0 = in_x1;
					in_y0 = in_y1;
					if (in_index < numPoints[InParamOffset])
					{
						in_q[InParamOffset]->getPoint(in_index, in_x1, in_y1);
						if (in_x1 < in_x0) in_x1 = in_x0; // should never happen (host served invalid point queue)
						else if (in_x1 >= data.numSamples) in_x1 = data.numSamples - 1; // should never happen (host served invalid point queue)
						++in_index;
						CONSTRAIN(in_y1);
					}
					else
					{
						in_x1 = data.numSamples - 1;
						break;
					}
				}
				// Postcondition: in_x0 <= out_x0 < in_x1 < numSamples
				// Postcondition: in_x0 <= out_x0 < out_x1 < numSamples

				// Find the first point of Slowness's automation curve that is strictly after out_x0
				while (slowness_x <= out_x0)
				{
					if (slowness_index < numPoints[SlownessOffset])
					{
						in_q[SlownessOffset]->getPoint(slowness_index, slowness_x, slowness);
						++slowness_index;
						if (slowness_x >= data.numSamples) slowness_x = data.numSamples - 1; // should never happen (host served invalid point queue)
						CONSTRAIN(slowness);
					}
					else
					{
						slowness_x = data.numSamples - 1;
						break;
					}
				}
				// Postcondition: out_x0 < slowness_x < numSamples
				// Postcondition: in_x0 <= out_x0 < in_x1 < numSamples
				// Postcondition: in_x0 <= out_x0 < out_x1 < numSamples

				// Let x be the first sample offset within (out_x0,out_x1] where InParam or Slowness changes
				// (or let x = out_x1 if neither changes anywhere within that interval).
				int32 x = (in_x1 <= slowness_x) ? in_x1 : slowness_x;
				if (x > out_x1) x = out_x1;
				// Postcondition: out_x0 < x <= out_x1 < numSamples

				// Prepare to output a new OutParam automation curve point at x...
				// Note:  in_x1 - in_x0 > 0 because in_x0 <= out_x0 < in_x1
				ParamValue max_slope =
					(slowness <= 0.) ? 1. : ((1. - slowness) / slowness / secs_per_half_slowness / data.processContext->sampleRate);
				const ParamValue in_slope = (in_y1 - in_y0) / (ParamValue)(in_x1 - in_x0);
				in_y0 = interpolate(in_x0, in_y0, in_x1, in_y1, out_x0);
				in_x0 = out_x0;
				ParamValue param_diff = in_y0 - out_y0;
				if (param_diff < 0.)
					param_diff = -param_diff;
				ParamValue out_slope, y;

				// If OutParam can catch the InParam's automation curve (without exceeding speed max_slope) before x,
				// output an extra automation curve point for OutParam at the intersection point of the two curves.
				// Otherwise move it toward InParam at its max allowed speed.
				if (param_diff > small_double)
				{
					out_slope = (in_y0 > out_y0) ? max_slope : -max_slope;
					const int32 intersection_x = (in_slope == out_slope) ? -1
						: (out_x0 + (int32)std::round((in_y0 - out_y0) / (out_slope - in_slope)));
					if (out_x0 < intersection_x && intersection_x < x)
					{
						ParamValue intersection_y = in_y0 + in_slope * (ParamValue)(intersection_x - out_x0);
						CONSTRAIN(intersection_y);
						addOutPoint(data, out_queue[param_set], param_set, out_x0, intersection_x, lastCC, intersection_y);
						out_x0 = in_x0 = intersection_x;
						out_y0 = in_y0 = intersection_y;
						param_diff = 0.;
					}
					else
					{
						y = out_y0 + out_slope * (ParamValue)(x - out_x0);
					}
				}

				// If OutParam has already reached InParam, make it follow InParam's movement up to its max allowed speed.
				if (param_diff <= small_double)
				{
					if (in_slope < -max_slope)
					{
						out_slope = -max_slope;
						y = in_y0 + out_slope * (ParamValue)(x - out_x0);
					}
					else if (in_slope <= max_slope)
					{
						out_slope = in_slope;
						y = interpolate(in_x0, in_y0, in_x1, in_y1, x);
					}
					else
					{
						out_slope = max_slope;
						y = in_y0 + out_slope * (ParamValue)(x - out_x0);
					}
				}

				CONSTRAIN(y);

				// Output the computed automation curve point for OutParam (but omit it if it's at the
				// end of a flat segment of the curve at the end of the buffer, as per the VST3 standard).
				if (!(x >= data.numSamples - 1 && roughly_equal(out_y0, y)))
					addOutPoint(data, out_queue[param_set], param_set, out_x0, x, lastCC, y);

				// Shift out_x0 forward to the most recently outputted point, and continue until the
				// end of the non-overridden OutParam segment is reached.
				out_x0 = x;
				out_y0 = y;
			}
			// Postcondition: out_x0 == out_x1

			// Point (out_x1, ?) is the boundary between the end of a VST-generated smoothed curve and a
			// host-generated segment that overrides the VST's curve.  For best smoothing, we interpret
			// this point as having y-value equal to the VST-generated smoothed curve's value, so that the
			// overridden segment starts as a 0% override and linearly progresses to a 100% override by
			// the end of the overridden segment.  When the override is a jump in the curve (common case),
			// this preserves the jump; but when the override is a gradual change (e.g., introduced by
			// host automation), this replaces the overridden segment with a smooth transition from the
			// VST-generated smoothed segment's end to the host-generated overridden segment's end.
			out_y1 = out_y0;

			// Now continue with the next segment (if any) of the incoming OutParam automation curve
			// (which will always be an overridden segment if we got this far in the loop body)...
		}

		// Force-output points at sample offset 0 on the first call to process(), to help hosts
		// synchronize their parameters with the VST's after a load/restore of plug-in state.
		if (!initial_points_sent && data.outputParameterChanges)
		{
			output_initial_point(data.outputParameterChanges, param_set * NumParamOffsets + OutParamOffset, saved_original_outval);
			if (numPoints[InParamOffset] <= 0)
				output_initial_point(data.outputParameterChanges, param_set * NumParamOffsets + InParamOffset, values[param_set].in);
			if (numPoints[SlownessOffset] <= 0)
				output_initial_point(data.outputParameterChanges, param_set * NumParamOffsets + SlownessOffset, values[param_set].slowness);
		}

		// Update the stored values of InParam and Slowness for use by the next call to process().
		if (numPoints[InParamOffset] > 0)
		{
			int32 dummy;
			in_q[InParamOffset]->getPoint(numPoints[InParamOffset] - 1, dummy, values[param_set].in);
		}
		if (numPoints[SlownessOffset] > 0)
		{
			int32 dummy;
			in_q[SlownessOffset]->getPoint(numPoints[SlownessOffset] - 1, dummy, values[param_set].slowness);
		}
	}

	if (data.outputParameterChanges)
		initial_points_sent = true;

	return kResultOk;
}
