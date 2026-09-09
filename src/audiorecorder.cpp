#include "audiorecorder.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cstdint>
#endif

// Standard WAV header struct
#pragma pack(push, 1)
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t formatType = 1; // PCM
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t bytesPerSec = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)

#ifdef Q_OS_WIN
#include <QVector>

// Helper to convert float or int data to 16-bit PCM at 16000Hz mono and append to buffer
static void resampleAndMixTo16kMono(
    QByteArray& outBuffer,
    const BYTE* pData,
    UINT32 numFrames,
    const WAVEFORMATEX* pwfx,
    double& sampleTimeOffset
) {
    int channels = pwfx->nChannels;
    int inSampleRate = pwfx->nSamplesPerSec;
    
    // 1. Convert the incoming packet to a mono float array [-1.0 ... 1.0]
    QVector<float> inMono;
    inMono.reserve(numFrames);
    
    bool isFloat = false;
    if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        isFloat = true;
    } else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* pEx = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pwfx);
        // Compare GUID for IEEE float: {00000003-0000-0010-8000-00aa00389b71}
        if (pEx->SubFormat.Data1 == 0x00000003 && 
            pEx->SubFormat.Data2 == 0x0000 && 
            pEx->SubFormat.Data3 == 0x0010 &&
            pEx->SubFormat.Data4[0] == 0x80 && pEx->SubFormat.Data4[1] == 0x00 &&
            pEx->SubFormat.Data4[2] == 0x00 && pEx->SubFormat.Data4[3] == 0xaa &&
            pEx->SubFormat.Data4[4] == 0x00 && pEx->SubFormat.Data4[5] == 0x38 &&
            pEx->SubFormat.Data4[6] == 0x9b && pEx->SubFormat.Data4[7] == 0x71) 
        {
            isFloat = true;
        }
    }
    
    if (isFloat) {
        const float* fData = reinterpret_cast<const float*>(pData);
        for (UINT32 i = 0; i < numFrames; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c) {
                sum += fData[i * channels + c];
            }
            inMono.append(sum / channels);
        }
    } else {
        if (pwfx->wBitsPerSample == 16) {
            const int16_t* sData = reinterpret_cast<const int16_t*>(pData);
            for (UINT32 i = 0; i < numFrames; ++i) {
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    sum += static_cast<float>(sData[i * channels + c]) / 32768.0f;
                }
                inMono.append(sum / channels);
            }
        } else if (pwfx->wBitsPerSample == 32) {
            const int32_t* iData = reinterpret_cast<const int32_t*>(pData);
            for (UINT32 i = 0; i < numFrames; ++i) {
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    sum += static_cast<float>(iData[i * channels + c]) / 2147483648.0f;
                }
                inMono.append(sum / channels);
            }
        } else if (pwfx->wBitsPerSample == 8) {
            const uint8_t* uData = reinterpret_cast<const uint8_t*>(pData);
            for (UINT32 i = 0; i < numFrames; ++i) {
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    sum += (static_cast<float>(uData[i * channels + c]) - 128.0f) / 128.0f;
                }
                inMono.append(sum / channels);
            }
        } else {
            inMono.fill(0.0f, numFrames);
        }
    }
    
    // 2. Resample mono float to 16000Hz using linear interpolation
    double step = static_cast<double>(inSampleRate) / 16000.0;
    double idx = sampleTimeOffset;
    
    while (idx < numFrames) {
        int idx1 = static_cast<int>(idx);
        int idx2 = idx1 + 1;
        float val = 0.0f;
        
        if (idx2 < numFrames) {
            float frac = static_cast<float>(idx - idx1);
            val = inMono[idx1] * (1.0f - frac) + inMono[idx2] * frac;
        } else if (idx1 < numFrames) {
            val = inMono[idx1];
        }
        
        if (val > 1.0f) val = 1.0f;
        else if (val < -1.0f) val = -1.0f;
        
        int16_t pcmVal = static_cast<int16_t>(val * 32767.0f);
        outBuffer.append(reinterpret_cast<const char*>(&pcmVal), sizeof(pcmVal));
        
        idx += step;
    }
    
    sampleTimeOffset = idx - numFrames;
}
#endif // Q_OS_WIN

LoopbackRecorderWorker::LoopbackRecorderWorker(const QString& filePath, QObject* parent)
    : QThread(parent)
    , m_filePath(filePath)
{
}

void LoopbackRecorderWorker::stop() {
    QMutexLocker locker(&m_mutex);
    m_running = false;
}

QByteArray LoopbackRecorderWorker::getAccumulatedPcm() {
    QMutexLocker locker(&m_mutex);
    return m_pcmData;
}

void LoopbackRecorderWorker::run() {
#ifndef Q_OS_WIN
    emit errorOccurred("Dual audio capture is only supported on Windows.");
#else
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        emit errorOccurred("Failed to initialize COM library.");
        return;
    }

    IMMDeviceEnumerator* pEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        CoUninitialize();
        emit errorOccurred("Failed to create MMDeviceEnumerator.");
        return;
    }

    // --- Device 1: Render Loopback (Speakers/Headphones) ---
    IMMDevice* pRenderDevice = NULL;
    IAudioClient* pRenderClient = NULL;
    WAVEFORMATEX* pRenderWfx = NULL;
    IAudioCaptureClient* pRenderCapture = NULL;
    bool renderOk = false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDevice);
    if (SUCCEEDED(hr)) {
        hr = pRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pRenderClient);
        if (SUCCEEDED(hr)) {
            hr = pRenderClient->GetMixFormat(&pRenderWfx);
            if (SUCCEEDED(hr)) {
                hr = pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pRenderWfx, NULL);
                if (SUCCEEDED(hr)) {
                    hr = pRenderClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pRenderCapture);
                    if (SUCCEEDED(hr)) {
                        hr = pRenderClient->Start();
                        if (SUCCEEDED(hr)) {
                            renderOk = true;
                        }
                    }
                }
            }
        }
    }

    // --- Device 2: Capture Device (Microphone) ---
    IMMDevice* pMicDevice = NULL;
    IAudioClient* pMicClient = NULL;
    WAVEFORMATEX* pMicWfx = NULL;
    IAudioCaptureClient* pMicCapture = NULL;
    bool micOk = false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pMicDevice);
    if (SUCCEEDED(hr)) {
        hr = pMicDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pMicClient);
        if (SUCCEEDED(hr)) {
            hr = pMicClient->GetMixFormat(&pMicWfx);
            if (SUCCEEDED(hr)) {
                hr = pMicClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pMicWfx, NULL);
                if (SUCCEEDED(hr)) {
                    hr = pMicClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pMicCapture);
                    if (SUCCEEDED(hr)) {
                        hr = pMicClient->Start();
                        if (SUCCEEDED(hr)) {
                            micOk = true;
                        }
                    }
                }
            }
        }
    }

    // If neither device is active, we cannot record audio
    if (!renderOk && !micOk) {
        if (pRenderWfx) CoTaskMemFree(pRenderWfx);
        if (pRenderClient) pRenderClient->Release();
        if (pRenderDevice) pRenderDevice->Release();
        if (pMicWfx) CoTaskMemFree(pMicWfx);
        if (pMicClient) pMicClient->Release();
        if (pMicDevice) pMicDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        emit errorOccurred("No active speaker output or microphone input device found.");
        return;
    }

    double renderOffset = 0.0;
    double micOffset = 0.0;
    QByteArray render16kBuf;
    QByteArray mic16kBuf;

    // Clear old buffer
    {
        QMutexLocker locker(&m_mutex);
        m_pcmData.clear();
        m_pcmData.reserve(4 * 1024 * 1024); // Preallocate ~4MB
    }

    bool shouldStop = false;
    while (!shouldStop) {
        {
            QMutexLocker locker(&m_mutex);
            shouldStop = !m_running;
        }

        // 1. Read Loopback (Speakers)
        if (renderOk) {
            UINT32 packetLength = 0;
            hr = pRenderCapture->GetNextPacketSize(&packetLength);
            while (SUCCEEDED(hr) && packetLength > 0) {
                BYTE* pData;
                UINT32 numFramesRead;
                DWORD flags;
                hr = pRenderCapture->GetBuffer(&pData, &numFramesRead, &flags, NULL, NULL);
                if (SUCCEEDED(hr)) {
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                        resampleAndMixTo16kMono(render16kBuf, pData, numFramesRead, pRenderWfx, renderOffset);
                    } else {
                        QByteArray silent(numFramesRead * pRenderWfx->nBlockAlign, 0);
                        resampleAndMixTo16kMono(render16kBuf, reinterpret_cast<const BYTE*>(silent.constData()), numFramesRead, pRenderWfx, renderOffset);
                    }
                    pRenderCapture->ReleaseBuffer(numFramesRead);
                }
                hr = pRenderCapture->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        }

        // 2. Read Microphone
        if (micOk) {
            UINT32 packetLength = 0;
            hr = pMicCapture->GetNextPacketSize(&packetLength);
            while (SUCCEEDED(hr) && packetLength > 0) {
                BYTE* pData;
                UINT32 numFramesRead;
                DWORD flags;
                hr = pMicCapture->GetBuffer(&pData, &numFramesRead, &flags, NULL, NULL);
                if (SUCCEEDED(hr)) {
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                        resampleAndMixTo16kMono(mic16kBuf, pData, numFramesRead, pMicWfx, micOffset);
                    } else {
                        QByteArray silent(numFramesRead * pMicWfx->nBlockAlign, 0);
                        resampleAndMixTo16kMono(mic16kBuf, reinterpret_cast<const BYTE*>(silent.constData()), numFramesRead, pMicWfx, micOffset);
                    }
                    pMicCapture->ReleaseBuffer(numFramesRead);
                }
                hr = pMicCapture->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        }

        // 3. Resample and Mix
        if (renderOk && micOk) {
            int availableSamples = qMin(render16kBuf.size(), mic16kBuf.size()) / 2;
            if (availableSamples > 0) {
                const int16_t* renderSamples = reinterpret_cast<const int16_t*>(render16kBuf.constData());
                const int16_t* micSamples = reinterpret_cast<const int16_t*>(mic16kBuf.constData());
                
                QByteArray mixedChunk;
                mixedChunk.reserve(availableSamples * 2);
                
                for (int i = 0; i < availableSamples; ++i) {
                    int32_t mixed = static_cast<int32_t>(renderSamples[i]) + static_cast<int32_t>(micSamples[i]);
                    // Clamp to 16-bit signed int range
                    if (mixed > 32767) mixed = 32767;
                    else if (mixed < -32768) mixed = -32768;
                    
                    int16_t mixedVal = static_cast<int16_t>(mixed);
                    mixedChunk.append(reinterpret_cast<const char*>(&mixedVal), 2);
                }
                
                {
                    QMutexLocker locker(&m_mutex);
                    m_pcmData.append(mixedChunk);
                }
                
                render16kBuf.remove(0, availableSamples * 2);
                mic16kBuf.remove(0, availableSamples * 2);
            }
        } else if (renderOk) {
            if (!render16kBuf.isEmpty()) {
                {
                    QMutexLocker locker(&m_mutex);
                    m_pcmData.append(render16kBuf);
                }
                render16kBuf.clear();
            }
        } else if (micOk) {
            if (!mic16kBuf.isEmpty()) {
                {
                    QMutexLocker locker(&m_mutex);
                    m_pcmData.append(mic16kBuf);
                }
                mic16kBuf.clear();
            }
        }

        QThread::msleep(10);
    }

    // Stop and Clean Up Render Device
    if (renderOk) {
        pRenderClient->Stop();
        pRenderCapture->Release();
        CoTaskMemFree(pRenderWfx);
        pRenderClient->Release();
        pRenderDevice->Release();
    }

    // Stop and Clean Up Mic Device
    if (micOk) {
        pMicClient->Stop();
        pMicCapture->Release();
        CoTaskMemFree(pMicWfx);
        pMicClient->Release();
        pMicDevice->Release();
    }

    pEnumerator->Release();
    CoUninitialize();

    // Save mixed output to WAV file (always 16kHz mono)
    QByteArray finalPcm;
    {
        QMutexLocker locker(&m_mutex);
        finalPcm = m_pcmData;
    }
    AudioRecorder::saveWavFile(m_filePath, finalPcm, 16000, 1);

    emit finishedSuccessfully(m_filePath);
#endif // Q_OS_WIN
}

// ─────────────────────────────────────────────────────────────────────────────
// AudioRecorder Class implementation
// ─────────────────────────────────────────────────────────────────────────────

AudioRecorder::AudioRecorder(QObject* parent)
    : QObject(parent)
{
}

AudioRecorder::~AudioRecorder() {
    stopRecording();
}

void AudioRecorder::startRecording(const QString& filePath) {
    if (m_isRecording) return;
    
    m_isRecording = true;
    m_worker = new LoopbackRecorderWorker(filePath, this);
    
    connect(m_worker, &LoopbackRecorderWorker::finishedSuccessfully, this, &AudioRecorder::onWorkerFinished);
    connect(m_worker, &LoopbackRecorderWorker::errorOccurred,       this, &AudioRecorder::onWorkerError);
    connect(m_worker, &QThread::finished,                            m_worker, &QObject::deleteLater);
    
    m_worker->start();
}

void AudioRecorder::stopRecording() {
    if (!m_isRecording || !m_worker) return;
    
    m_worker->stop();
    m_worker->wait();
    m_worker = nullptr;
    m_isRecording = false;
}

void AudioRecorder::onWorkerFinished(const QString& filePath) {
    emit recordingFinished(filePath);
}

void AudioRecorder::onWorkerError(const QString& error) {
    emit errorOccurred(error);
}

QByteArray AudioRecorder::getAccumulatedPcm() {
    if (m_worker && m_isRecording) {
        return m_worker->getAccumulatedPcm();
    }
    return QByteArray();
}

void AudioRecorder::saveWavFile(const QString& filePath, const QByteArray& pcmData, uint32_t sampleRate, uint16_t channels) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open output WAV file for writing:" << filePath;
        return;
    }
    
    WAVHeader header;
    header.channels = channels;
    header.sampleRate = sampleRate;
    header.bitsPerSample = 16;
    header.blockAlign = channels * 2;
    header.bytesPerSec = sampleRate * header.blockAlign;
    header.dataSize = pcmData.size();
    header.fileSize = sizeof(WAVHeader) - 8 + pcmData.size();
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));
    file.write(pcmData);
    file.close();
}
