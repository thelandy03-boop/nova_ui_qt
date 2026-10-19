#include "NovaAndroidAudioEngine.h"

#if defined(Q_OS_ANDROID)
#include <oboe/Oboe.h>

#define LOG_TAG "NovaAndroidAudio"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) 
#define LOGE(...)
#define LOGW(...)
#endif

// ARDOUR includes for Android (using stubs)
#include "NovaAndroidStubs.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

// ============================================================================
// WAV Header (RIFF IEEE Float 32-bit)
// ============================================================================
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 3; // IEEE Float
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

// ============================================================================
// RingBuffer Implementation
// ============================================================================

NovaAndroidAudioEngine::RingBuffer::RingBuffer(size_t capacityFrames, int channels)
    : m_buffer(capacityFrames * channels)
    , m_capacityFrames(capacityFrames)
    , m_channels(channels)
{
}

NovaAndroidAudioEngine::RingBuffer::~RingBuffer() = default;

bool NovaAndroidAudioEngine::RingBuffer::write(const float* input, size_t frames) {
    if (frames == 0) return true;
    
    const size_t samples = frames * m_channels;
    size_t available = availableWrite();
    if (samples > available) {
        // Overrun - drop oldest data or return false
        // LOGE removed from realtime path - use atomic flag instead
        m_overrun.store(true, std::memory_order_relaxed);
        return false;
    }

    size_t writePos = m_writePos.load(std::memory_order_relaxed);
    size_t firstChunk = std::min(samples, m_buffer.size() - writePos);
    
    std::copy(input, input + firstChunk, m_buffer.begin() + writePos);
    
    if (firstChunk < samples) {
        std::copy(input + firstChunk, input + samples, m_buffer.begin());
    }
    
    m_writePos.store((writePos + samples) % m_buffer.size(), std::memory_order_release);
    return true;
}

size_t NovaAndroidAudioEngine::RingBuffer::read(float* output, size_t frames) {
    if (frames == 0) return 0;
    
    const size_t samples = frames * m_channels;
    size_t available = availableRead();
    size_t toRead = std::min(samples, available);
    
    if (toRead == 0) {
        m_underrun.store(true, std::memory_order_relaxed);
        return 0;
    }

    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    size_t firstChunk = std::min(toRead, m_buffer.size() - readPos);
    
    std::copy(m_buffer.begin() + readPos, m_buffer.begin() + readPos + firstChunk, output);
    
    if (firstChunk < toRead) {
        std::copy(m_buffer.begin(), m_buffer.begin() + (toRead - firstChunk), output + firstChunk);
    }
    
    m_readPos.store((readPos + toRead) % m_buffer.size(), std::memory_order_release);
    return toRead / m_channels;
}

size_t NovaAndroidAudioEngine::RingBuffer::availableRead() const {
    size_t writePos = m_writePos.load(std::memory_order_acquire);
    size_t readPos = m_readPos.load(std::memory_order_relaxed);
    
    if (writePos >= readPos) {
        return writePos - readPos;
    } else {
        return m_buffer.size() - (readPos - writePos);
    }
}

size_t NovaAndroidAudioEngine::RingBuffer::availableWrite() const {
    return m_buffer.size() - availableRead() - 1; // -1 to distinguish full/empty
}

void NovaAndroidAudioEngine::RingBuffer::clear() {
    m_writePos.store(0, std::memory_order_release);
    m_readPos.store(0, std::memory_order_release);
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
}

// ============================================================================
// NovaAndroidAudioEngine Implementation
// ============================================================================

NovaAndroidAudioEngine::NovaAndroidAudioEngine(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_ANDROID)
    m_inputCallback = std::make_unique<InputStreamCallback>(this);
    m_outputCallback = std::make_unique<OutputStreamCallback>(this);
#endif
}

NovaAndroidAudioEngine::~NovaAndroidAudioEngine() {
    shutdown();
}

bool NovaAndroidAudioEngine::initialize(int sampleRate, int framesPerBurst) {
#if defined(Q_OS_ANDROID)
    m_sampleRate = sampleRate;
    
    // Get optimal frames per burst from Oboe
    if (framesPerBurst == 0) {
        // We'll query this after opening streams
        m_framesPerBurst = 256; // default fallback
    } else {
        m_framesPerBurst = framesPerBurst;
    }

    // Open output stream first (lower latency requirement)
    // Note: Oboe builder setters return AudioStreamBuilder* (pointer), hence -> chaining
    oboe::AudioStreamBuilder outputBuilder;
    outputBuilder.setDirection(oboe::Direction::Output);
    outputBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    outputBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    outputBuilder.setFormat(oboe::AudioFormat::Float);
    outputBuilder.setChannelCount(2);
    outputBuilder.setSampleRate(sampleRate);
    outputBuilder.setCallback(m_outputCallback.get());

    oboe::Result result = outputBuilder.openStream(m_outputStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        Q_EMIT audioInitialized(false);
        return false;
    }

    m_framesPerBurst = m_outputStream->getFramesPerBurst();
    m_framesPerBurst = std::max(m_framesPerBurst, 64);
    LOGD("Output stream opened: %d Hz, %d frames/burst, %d channels", 
         m_outputStream->getSampleRate(), m_framesPerBurst, m_outputStream->getChannelCount());

    // Open input stream
    oboe::AudioStreamBuilder inputBuilder;
    inputBuilder.setDirection(oboe::Direction::Input);
    inputBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    inputBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    inputBuilder.setFormat(oboe::AudioFormat::Float);
    inputBuilder.setChannelCount(2);
    inputBuilder.setSampleRate(sampleRate);
    inputBuilder.setFramesPerCallback(m_framesPerBurst);
    inputBuilder.setCallback(m_inputCallback.get());

    result = inputBuilder.openStream(m_inputStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open input stream: %s", oboe::convertToText(result));
        m_outputStream->close();
        Q_EMIT audioInitialized(false);
        return false;
    }

    LOGD("Input stream opened: %d Hz, %d frames/burst, %d channels",
         m_inputStream->getSampleRate(), m_inputStream->getFramesPerBurst(), m_inputStream->getChannelCount());

    // Initialize ring buffers
    int maxTracks = 32; // configurable
    int outputChannels = m_outputStream->getChannelCount();
    int inputChannels = m_inputStream->getChannelCount();
    size_t bufferCapacity = static_cast<size_t>(m_framesPerBurst) * 4; // 4 bursts worth

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_inputBuffers.clear();
        m_outputBuffers.clear();
        
        for (int i = 0; i < maxTracks; ++i) {
            m_inputBuffers.push_back(std::make_unique<RingBuffer>(bufferCapacity, inputChannels));
            m_outputBuffers.push_back(std::make_unique<RingBuffer>(bufferCapacity, outputChannels));
        }
    }

    // Start disk writer thread
    m_diskWriterRunning = true;
    m_diskWriterThread = std::thread(&NovaAndroidAudioEngine::diskWriterLoop, this);

    Q_EMIT audioInitialized(true);
    LOGD("Audio engine initialized successfully");
    return true;

#else
    Q_UNUSED(sampleRate);
    Q_UNUSED(framesPerBurst);
    Q_EMIT audioInitialized(false);
    return false;
#endif
}

void NovaAndroidAudioEngine::shutdown() {
#if defined(Q_OS_ANDROID)
    stop();
    
    m_diskWriterRunning = false;
    m_diskWriterCV.notify_all();
    if (m_diskWriterThread.joinable()) {
        m_diskWriterThread.join();
    }

    closeStreams();
    
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_inputBuffers.clear();
        m_outputBuffers.clear();
    }
    
    m_session = nullptr;
#endif
}

bool NovaAndroidAudioEngine::start() {
#if defined(Q_OS_ANDROID)
    if (!m_outputStream || !m_inputStream) return false;
    
    oboe::Result result = m_outputStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start output stream: %s", oboe::convertToText(result));
        return false;
    }
    
    result = m_inputStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start input stream: %s", oboe::convertToText(result));
        m_outputStream->requestStop();
        return false;
    }
    
    m_isPlaying = true;
    m_isRecording = true;
    m_currentFrame = 0;
    Q_EMIT playbackStateChanged(true);
    Q_EMIT recordingStateChanged(true);
    return true;
#else
    return false;
#endif
}

void NovaAndroidAudioEngine::stop() {
#if defined(Q_OS_ANDROID)
    m_isPlaying = false;
    m_isRecording = false;
    
    if (m_outputStream) {
        m_outputStream->requestStop();
        m_outputStream->close();
        m_outputStream.reset();
    }
    
    if (m_inputStream) {
        m_inputStream->requestStop();
        m_inputStream->close();
        m_inputStream.reset();
    }
    
    Q_EMIT playbackStateChanged(false);
    Q_EMIT recordingStateChanged(false);
#endif
}

void NovaAndroidAudioEngine::setSession(ARDOUR::Session* session) {
    m_session = session;
}

bool NovaAndroidAudioEngine::openInputStream(int channelCount) {
#if defined(Q_OS_ANDROID)
    return m_inputStream != nullptr && m_inputStream->getChannelCount() >= channelCount;
#else
    Q_UNUSED(channelCount);
    return false;
#endif
}

bool NovaAndroidAudioEngine::openOutputStream(int channelCount) {
#if defined(Q_OS_ANDROID)
    return m_outputStream != nullptr && m_outputStream->getChannelCount() >= channelCount;
#else
    Q_UNUSED(channelCount);
    return false;
#endif
}

void NovaAndroidAudioEngine::closeStreams() {
#if defined(Q_OS_ANDROID)
    if (m_outputStream) {
        m_outputStream->close();
        m_outputStream.reset();
    }
    if (m_inputStream) {
        m_inputStream->close();
        m_inputStream.reset();
    }
#endif
}

void NovaAndroidAudioEngine::setRecording(bool enabled) {
    m_isRecording.store(enabled);
    Q_EMIT recordingStateChanged(enabled);
}

void NovaAndroidAudioEngine::setPlaying(bool enabled) {
    m_isPlaying.store(enabled);
    Q_EMIT playbackStateChanged(enabled);
}

NovaAndroidAudioEngine::RingBuffer* NovaAndroidAudioEngine::getInputBuffer(int trackIndex) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_inputBuffers.size())) {
        return m_inputBuffers[trackIndex].get();
    }
    return nullptr;
}

NovaAndroidAudioEngine::RingBuffer* NovaAndroidAudioEngine::getOutputBuffer(int trackIndex) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_outputBuffers.size())) {
        return m_outputBuffers[trackIndex].get();
    }
    return nullptr;
}

void NovaAndroidAudioEngine::diskWriterLoop() {
#if defined(Q_OS_ANDROID)
    while (m_diskWriterRunning) {
        std::unique_lock<std::mutex> lock(m_diskWriterMutex);
        m_diskWriterCV.wait_for(lock, std::chrono::milliseconds(100), [this]() {
            return !m_diskWriterRunning;
        });
        
        if (!m_diskWriterRunning) break;
        
        // Write each track's buffer to disk
        if (m_session) {
            auto routes = m_session->get_routes();
            if (routes) {
                int trackIdx = 0;
                for (const auto& route : *routes) {
                    if (route && route->is_track()) {
                        writeTrackToDisk(trackIdx);
                        trackIdx++;
                    }
                }
            }
        }
    }
#endif
}

void NovaAndroidAudioEngine::writeTrackToDisk(int trackIndex) {
#if defined(Q_OS_ANDROID)
    auto* inputBuf = getInputBuffer(trackIndex);
    auto* outputBuf = getOutputBuffer(trackIndex);
    
    if (!inputBuf && !outputBuf) return;
    
    // Monitor buffer levels
    if (inputBuf) {
        size_t avail = inputBuf->availableRead();
        if (avail > inputBuf->capacity() * 0.9) {
            Q_EMIT bufferOverrun();
        }
    }
    if (outputBuf) {
        size_t avail = outputBuf->availableRead();
        if (avail > outputBuf->capacity() * 0.9) {
            Q_EMIT bufferUnderrun();
        }
    }
    
    // Only write input buffers (recorded audio) to disk
    if (!inputBuf) return;
    
    size_t avail = inputBuf->availableRead();
    if (avail == 0) return;
    
    // Get track info from session
    if (!m_session) return;
    auto routes = m_session->get_routes();
    if (!routes) return;
    
    int trackIdx = 0;
    std::shared_ptr<ARDOUR::Track> targetTrack = nullptr;
    for (const auto& route : *routes) {
        if (route && route->is_track()) {
            if (trackIdx == trackIndex) {
                targetTrack = std::dynamic_pointer_cast<ARDOUR::Track>(route);
                break;
            }
            trackIdx++;
        }
    }
    
    if (!targetTrack) return;
    
    // Read available samples from ring buffer
    size_t samplesToRead = avail / inputBuf->channels();
    std::vector<float> buffer(samplesToRead * inputBuf->channels());
    size_t framesRead = inputBuf->read(buffer.data(), samplesToRead);
    
    if (framesRead == 0) return;
    
    // Write to WAV file in project interchange directory
    writeWavFile(targetTrack, buffer.data(), framesRead, inputBuf->channels());
#else
    Q_UNUSED(trackIndex);
#endif
}

void NovaAndroidAudioEngine::writeWavFile(
    std::shared_ptr<ARDOUR::Track> track,
    const float* samples,
    size_t frames,
    int channels) {
#if defined(Q_OS_ANDROID)
    if (!m_session) return;
    
    // Create interchange directory
    std::string sessionPath = m_session->path();
    std::string interchangeDir = sessionPath + "/interchange/" + track->name();
    
    // Create directory if needed
    std::string cmd = "mkdir -p " + interchangeDir;
    system(cmd.c_str());
    
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* tm = localtime(&time_t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);
    
    std::string filename = interchangeDir + "/" + track->name() + "_" + timestamp + ".wav";
    
    // Open file for appending (or create new)
    FILE* file = fopen(filename.c_str(), "ab");
    if (!file) {
        LOGE("Failed to open WAV file for writing: %s", filename.c_str());
        return;
    }
    
    // Check if file is new (write header)
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    bool isNewFile = (fileSize == 0);
    
    size_t totalSamples = frames * channels;
    size_t dataBytes = totalSamples * sizeof(float);
    
    if (isNewFile) {
        // Write WAV header
        WavHeader header;
        header.sampleRate = m_sampleRate;
        header.channels = channels;
        header.bitsPerSample = 32; // float
        header.byteRate = header.sampleRate * header.channels * (header.bitsPerSample / 8);
        header.blockAlign = header.channels * (header.bitsPerSample / 8);
        header.dataSize = dataBytes;
        header.chunkSize = 36 + header.dataSize;
        
        fwrite(&header, sizeof(WavHeader), 1, file);
    } else {
        // Update existing header - need to update dataSize and chunkSize
        // Read current header
        fseek(file, 0, SEEK_SET);
        WavHeader existingHeader;
        fread(&existingHeader, sizeof(WavHeader), 1, file);
        
        // Update sizes
        existingHeader.dataSize += dataBytes;
        existingHeader.chunkSize = 36 + existingHeader.dataSize;
        
        // Write updated header
        fseek(file, 0, SEEK_SET);
        fwrite(&existingHeader, sizeof(WavHeader), 1, file);
        
        // Seek to end for appending
        fseek(file, 0, SEEK_END);
    }
    
    // Write audio data (32-bit IEEE float PCM)
    fwrite(samples, sizeof(float), totalSamples, file);
    
    fclose(file);
    
    LOGD("Wrote %zu frames to %s", frames, filename.c_str());
#else
    Q_UNUSED(track);
    Q_UNUSED(samples);
    Q_UNUSED(frames);
    Q_UNUSED(channels);
#endif
}

// ============================================================================
// Oboe Callbacks
// ============================================================================

#if defined(Q_OS_ANDROID)
oboe::DataCallbackResult NovaAndroidAudioEngine::InputStreamCallback::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    
    float* inputBuffer = static_cast<float*>(audioData);
    int channels = stream->getChannelCount();
    
    // Write to all armed track input buffers
    for (int i = 0; i < 32; ++i) { // max tracks
        auto* ringBuf = m_engine->getInputBuffer(i);
        if (ringBuf) {
            ringBuf->write(inputBuffer, numFrames);
        }
    }
    
    return oboe::DataCallbackResult::Continue;
}

void NovaAndroidAudioEngine::InputStreamCallback::onErrorAfterClose(
    oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Input stream error: %s", oboe::convertToText(error));
    m_engine->Q_EMIT bufferOverrun();
}

oboe::DataCallbackResult NovaAndroidAudioEngine::OutputStreamCallback::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    
    float* outputBuffer = static_cast<float*>(audioData);
    int channels = stream->getChannelCount();
    
    // Clear output buffer first
    std::fill(outputBuffer, outputBuffer + (numFrames * channels), 0.0f);
    
    // Read from track output buffers and mix
    // For simplicity, we'll read from track 0 output buffer
    // Real implementation would mix all tracks with gain/pan
    auto* track0Output = m_engine->getOutputBuffer(0);
    if (track0Output) {
        track0Output->read(outputBuffer, numFrames);
    }
    
    // Update frame counter
    m_engine->m_currentFrame.fetch_add(numFrames, std::memory_order_relaxed);
    
    return oboe::DataCallbackResult::Continue;
}

void NovaAndroidAudioEngine::OutputStreamCallback::onErrorAfterClose(
    oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Output stream error: %s", oboe::convertToText(error));
    m_engine->Q_EMIT bufferUnderrun();
}
#endif // Q_OS_ANDROID