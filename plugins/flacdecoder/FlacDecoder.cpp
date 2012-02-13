#include "FlacDecoder.h"
#include <iostream>
using namespace std;

FlacDecoder::FlacDecoder()
{
    m_pDecoder = FLAC__stream_decoder_new();
}

FlacDecoder::~FlacDecoder()
{
    FLAC__stream_decoder_delete(m_pDecoder);
}

void FlacDecoder::GetFileSuffix(vector<string>& list) const
{
    list.clear();
    list.push_back("flac");
}

ErrorCode FlacDecoder::Open(const string& url)
{
    FLAC__stream_decoder_set_md5_checking(m_pDecoder, false);
    FLAC__stream_decoder_init_file(
	    m_pDecoder,
	    url.c_str(),
	    &WriteCallback,
	    NULL,
	    &ErrorCallback,
	    NULL);

    if (!FLAC__stream_decoder_process_until_end_of_metadata(m_pDecoder))
	return MousDecoderFailedToOpen;

    if (!FLAC__stream_decoder_process_single(m_pDecoder))
	return MousDecoderFailedToOpen;

    m_Channels = FLAC__stream_decoder_get_channels(m_pDecoder);
    m_SampleRate = FLAC__stream_decoder_get_sample_rate(m_pDecoder);
    m_BitsPerSample = FLAC__stream_decoder_get_bits_per_sample(m_pDecoder);

    m_SampleCount = FLAC__stream_decoder_get_total_samples(m_pDecoder);
    m_Duration = m_SampleCount / FLAC__stream_decoder_get_sample_rate(m_pDecoder) * 1000.f;
    m_SampleIndex = 0;

    cout << m_SampleCount << endl;
    cout << m_Duration << endl;

    cout << m_Channels << endl;
    cout << m_SampleRate << endl;
    cout << m_BitsPerSample << endl;

    return MousOk;
}

void FlacDecoder::Close()
{
}

bool FlacDecoder::IsFormatVaild() const
{
    return true;
}

ErrorCode FlacDecoder::ReadUnit(char* data, uint32_t& used, uint32_t& unitCount)
{
    gBuf = data;
    if (FLAC__stream_decoder_process_single(m_pDecoder)) {
	used = gBufLen;
	unitCount = gSamplesRead;
	m_SampleIndex += gSamplesRead;
    } else {
	cout << "bad" << flush;
	used = 0;
	unitCount = 0;
    }
    return MousOk;
}

ErrorCode FlacDecoder::SetUnitIndex(uint64_t index)
{
    m_SampleIndex = index;
    FLAC__stream_decoder_seek_absolute(m_pDecoder, index);
    return MousOk;
}

uint32_t FlacDecoder::GetMaxBytesPerUnit() const
{
    return FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS * sizeof(uint32_t);
}

uint64_t FlacDecoder::GetUnitIndex() const
{
    return m_SampleIndex;
}

uint64_t FlacDecoder::GetUnitCount() const
{
    return m_SampleCount;
}

AudioMode FlacDecoder::GetAudioMode() const
{
    return MousStereo;
}

uint32_t FlacDecoder::GetChannels() const
{
    return m_Channels;
}

uint32_t FlacDecoder::GetBitsPerSample() const
{
    return m_BitsPerSample;
}

uint32_t FlacDecoder::GetSampleRate() const
{
    return m_SampleRate;
}

uint64_t FlacDecoder::GetDuration() const
{
    return m_Duration;
}

char* FlacDecoder::gBuf = NULL;
int32_t FlacDecoder::gBufLen = 0;
int32_t FlacDecoder::gSamplesRead = 0;

FLAC__StreamDecoderWriteStatus FlacDecoder::WriteCallback(
	    const FLAC__StreamDecoder *decoder, 
	    const FLAC__Frame *frame, 
	    const FLAC__int32 * const buffer[], 
	    void *client_data)
{
    const size_t& samples = frame->header.blocksize;
    const size_t& channels = frame->header.channels;

    if (gBuf != NULL) {

	for (size_t i = 0, sample = 0; sample < samples; ++sample) {
	    for (size_t channel = 0; channel < channels; ++channel, i+=2) {
		gBuf[i+1] = (buffer[channel][sample] >> 8);
		gBuf[i] = buffer[channel][sample];
	    }
	}
	gBufLen = (samples * channels) *2 ;

    }

    gSamplesRead = samples;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::ErrorCallback(
	const FLAC__StreamDecoder *decoder, 
	FLAC__StreamDecoderErrorStatus status, 
	void *client_data)
{

}
