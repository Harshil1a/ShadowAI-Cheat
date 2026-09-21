#include "aimanager.h"
#include "appconfig.h"
#include "accountmanager.h"
#include "screencapture.h"
#include "ocr.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>

AIManager::AIManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_currentReply && m_currentReply->isRunning()) {
            // Debug output removed for stealth
            m_currentReply->abort();
        }
    });
    loadKeys();
}

void AIManager::loadKeys() {
    // We now use AppConfig::instance().currentApiKey() directly in performRequest
    m_currentKeyIndex = AppConfig::instance().activeSlot();
}

// ─── Build request for OpenAI-compatible APIs ───────────────────────────────
static QJsonDocument buildOpenAIRequest(const QString& systemPrompt,
                                         const QString& userText,
                                         const QString& base64Image,
                                         const QString& model,
                                         bool stream)
{
    QJsonArray messages;

    // System message
    QJsonObject sys;
    sys["role"]    = "system";
    sys["content"] = systemPrompt;
    messages.append(sys);

    // User message
    QJsonObject user;
    user["role"] = "user";

    if (!base64Image.isEmpty()) {
        // Vision: content is array
        QJsonArray contentArr;

        if (!userText.isEmpty()) {
            QJsonObject textPart;
            textPart["type"] = "text";
            textPart["text"] = userText;
            contentArr.append(textPart);
        }

        QJsonObject imgPart;
        imgPart["type"] = "image_url";
        QJsonObject imgUrl;
        imgUrl["url"] = "data:image/jpeg;base64," + base64Image;
        imgUrl["detail"] = "auto";
        imgPart["image_url"] = imgUrl;
        contentArr.append(imgPart);

        user["content"] = contentArr;
    } else {
        user["content"] = userText;
    }

    messages.append(user);

    QJsonObject body;
    body["model"]       = model;
    body["messages"]    = messages;
    body["max_tokens"]  = AppConfig::instance().maxTokens();
    body["stream"]      = stream;

    return QJsonDocument(body);
}

// ─── Build Cloudflare Workers AI request (different from OpenAI format) ─────
static QJsonDocument buildCloudflareRequest(const QString& systemPrompt,
                                             const QString& userText,
                                             const QString& base64Image,
                                             const QString& model)
{
    QJsonArray messages;

    // System message
    if (!systemPrompt.isEmpty()) {
        QJsonObject sys;
        sys["role"]    = "system";
        sys["content"] = systemPrompt;
        messages.append(sys);
    }

    // User message — content is plain text, image goes in top-level "image" param
    QJsonObject user;
    user["role"]    = "user";
    user["content"] = userText.isEmpty() ? "Analyze the screenshot and respond according to your instructions." : userText;
    messages.append(user);

    QJsonObject body;
    body["model"]       = model;
    body["messages"]    = messages;
    body["max_tokens"]  = AppConfig::instance().maxTokens();
    body["stream"]      = false;  // Cloudflare Workers AI doesn't support SSE streaming

    // Cloudflare Workers AI uses a top-level "image" parameter, NOT image_url in content
    if (!base64Image.isEmpty()) {
        body["image"] = "data:image/jpeg;base64," + base64Image;
    }

    return QJsonDocument(body);
}

// ─── Build Gemini request ────────────────────────────────────────────────────
static QJsonDocument buildGeminiRequest(const QString& systemPrompt,
                                         const QString& userText,
                                         const QStringList& base64Images,
                                         const QString& base64Audio,
                                         const QString& audioMimeType)
{
    QJsonObject body;

    // System instruction (v1beta support)
    if (!systemPrompt.isEmpty()) {
        QJsonObject si;
        QJsonObject siPart;
        siPart["text"] = systemPrompt;
        QJsonArray siParts;
        siParts.append(siPart);
        si["parts"] = siParts;
        body["system_instruction"] = si;
    }

    QJsonArray contents;
    QJsonObject userContent;
    userContent["role"] = "user";
    
    QJsonArray parts;
    // Add text part
    QJsonObject tp;
    tp["text"] = userText;
    parts.append(tp);

    // Add image parts
    for (const QString& base64Image : base64Images) {
        if (!base64Image.isEmpty()) {
            QJsonObject ip;
            QJsonObject inlineData;
            inlineData["mimeType"] = "image/jpeg";
            inlineData["data"]     = base64Image;
            ip["inlineData"] = inlineData;
            parts.append(ip);
        }
    }

    // Add audio part if present
    if (!base64Audio.isEmpty()) {
        QJsonObject ap;
        QJsonObject inlineData;
        inlineData["mimeType"] = audioMimeType.isEmpty() ? "audio/wav" : audioMimeType;
        inlineData["data"]     = base64Audio;
        ap["inlineData"] = inlineData;
        parts.append(ap);
    }

    userContent["parts"] = parts;
    contents.append(userContent);
    body["contents"] = contents;

    QJsonObject genConfig;
    genConfig["maxOutputTokens"] = AppConfig::instance().maxTokens();
    genConfig["temperature"] = 0.7;
    body["generationConfig"] = genConfig;

    return QJsonDocument(body);
}

void AIManager::askText(const QString& userPrompt) {
    askWithImages(QList<QPixmap>(), userPrompt);
}

void AIManager::askWithImages(const QList<QPixmap>& screenshots, const QString& extraPrompt, const QString& base64Audio, const QString& audioMimeType) {
    if (m_busy) return; // Prevent stacking requests
    performRequest(screenshots, extraPrompt, base64Audio, audioMimeType);
}

void AIManager::performRequest(const QList<QPixmap>& screenshots, const QString& extraPrompt, const QString& base64Audio, const QString& audioMimeType) {
    if (m_currentReply) {
        m_currentReply->disconnect();
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    auto& cfg = AppConfig::instance();
    
    // Use the currently selected slot data
    QString apiKey      = cfg.currentApiKey();
    QString provider    = cfg.currentApiProvider();
    QString model       = cfg.currentApiModel();
    QString sysPrompt   = cfg.systemPrompt();

    // Cache parameters for potential retry
    m_lastScreenshots = screenshots;
    m_lastPrompt      = extraPrompt;
    m_lastBase64Audio = base64Audio;
    m_lastAudioMime   = audioMimeType;

    bool isProUser = cfg.isPro();
    bool useProCloud = isProUser && cfg.useProCloudEngine();

    QString customBaseUrl;
    if (useProCloud) {
        // PRO CLOUD ENGINE PROXY: Requests route securely through https://shadowtool.me/api/solve
        // The master API key remains 100% on the server and is never stored on the user's PC.
        provider = cfg.proCloudProvider();
        model    = cfg.proCloudModel();
        customBaseUrl = cfg.proCloudBaseUrl();
    } else {
        // CUSTOM / BYOK MODE or FREE REWARDED TIER
        if (!isProUser) {
            if (!cfg.canUseFreeQuery()) {
                emit errorOccurred("🔒 0 Solve Credits Remaining!\n\nTo unlock more AI solves, open Settings (Ctrl+S or tray icon) to bank credits.");
                return;
            }

            // Deduct 1 credit or 1 trial query silently without polluting exam chat
            if (cfg.freeCredits() > 0) {
                AccountManager::instance().consumeCredit();
                qDebug() << "[Credits] Consumed 1 solve credit. Remaining:" << cfg.freeCredits();
            } else {
                int used = cfg.recordFreeQuery();
                qDebug() << "[Credits] Free trial query used:" << used << "/ 3";
            }
        }

        if (apiKey.trimmed().isEmpty()) {
            emit errorOccurred("API key not set. Please configure your key in Settings or activate Pro.");
            return;
        } else {
            customBaseUrl = cfg.currentApiBaseUrl().trimmed();
        }
    }

    QStringList base64Images;
    for (const QPixmap& screenshot : screenshots) {
        if (!screenshot.isNull()) {
            // Use dynamic resolution from settings
            int res = AppConfig::instance().screenshotResolution();
            QPixmap scaled = screenshot;
            if (screenshot.width() > res || screenshot.height() > res) {
                scaled = screenshot.scaled(res, res, Qt::KeepAspectRatio, Qt::FastTransformation);
            }
            base64Images.append(ScreenCapture::pixmapToBase64(scaled, 30));



        }
    }

    // Use the user's custom system prompt from settings.
    // If no extra prompt is given, just pass a neutral message so the AI
    // follows the system prompt (set in Settings) instead of a hardcoded fallback.
    QString userText = extraPrompt;
    if (!screenshots.isEmpty()) {
        QString ocrText = OcrExtractor::extractText(screenshots).trimmed();
        if (!ocrText.isEmpty()) {
            if (userText.isEmpty()) {
                userText = "Here is the exact question and text extracted from the screen:\n\n" + ocrText + "\n\nProvide the complete, optimal answer and solution according to your instructions.";
            } else {
                userText += "\n\n[Captured Screen Text]:\n" + ocrText;
            }
        }
    }
    if (userText.isEmpty()) {
        userText = "Analyze the screenshot and respond according to your instructions.";
    }

    m_busy = true;
    m_fullResponse.clear();
    m_buffer.clear();
    m_activeProvider = provider;
    emit requestStarted();

    if (useProCloud) {
        // SECURE PRO CLOUD ENGINE PROXY:
        // Key stays 100% on server, checks license live in Supabase, and streams answer.
        QJsonObject payload;
        payload["email"] = cfg.userEmail();
        payload["hwid"] = AccountManager::instance().getMachineHwid();
        payload["prompt"] = userText;
        payload["system_prompt"] = sysPrompt;
        if (!model.isEmpty() && model != "gemini-2.5-flash") {
            payload["model"] = model;
        }

        QJsonArray imgArray;
        for (const QString& b64 : base64Images) {
            imgArray.append(b64);
        }
        payload["images"] = imgArray;

        if (!base64Audio.isEmpty()) {
            payload["audio"] = base64Audio;
            payload["audio_mime"] = audioMimeType;
        }

        QString proxyUrl = customBaseUrl.trimmed();
        if (proxyUrl.isEmpty() || !proxyUrl.startsWith("http") || proxyUrl.contains("generativelanguage") || proxyUrl.contains("cloudflare.com")) {
            proxyUrl = "https://shadowtool.me/api/solve";
        }

        QUrl url(proxyUrl);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("x-shadow-email", cfg.userEmail().toUtf8());
        req.setRawHeader("x-shadow-hwid", AccountManager::instance().getMachineHwid().toUtf8());

        m_currentReply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    } else if (provider == "openrouter") {
        // OpenRouter Enterprise Vision API
        QString firstImg = base64Images.isEmpty() ? "" : base64Images.first();
        QJsonDocument body = buildOpenAIRequest(sysPrompt, userText, firstImg,
                                                 model.isEmpty() ? "google/gemini-2.5-flash" : model,
                                                 true);
        QNetworkRequest req(QUrl("https://openrouter.ai/api/v1/chat/completions"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
        req.setRawHeader("HTTP-Referer", "https://shadow-ai-cheat.vercel.app");
        req.setRawHeader("X-Title", "ShadowAI Pro Engine");

        m_currentReply = m_nam->post(req, body.toJson());

    } else if (provider == "gemini") {
        // Use user selected model or default to gemini-2.0-flash
        if (model.isEmpty()) {
            model = "gemini-2.5-flash";
        }
        
        // Debug output removed for stealth

        // Gemini API
        QString endpoint = QString("https://generativelanguage.googleapis.com/v1beta/models/%1:streamGenerateContent?key=%2")
                               .arg(model)
                               .arg(apiKey);

        QJsonDocument body = buildGeminiRequest(sysPrompt, userText, base64Images, base64Audio, audioMimeType);

        QUrl url(endpoint);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        m_currentReply = m_nam->post(req, body.toJson());

    } else if (provider == "groq") {
        // Groq API (OpenAI-compatible)
        QString firstImg = base64Images.isEmpty() ? "" : base64Images.first();
        QJsonDocument body = buildOpenAIRequest(sysPrompt, userText, firstImg,
                                                 model.isEmpty() ? "meta-llama/llama-4-scout-17b-16e-instruct" : model,
                                                 true);
        QNetworkRequest req(QUrl("https://api.groq.com/openai/v1/chat/completions"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

        m_currentReply = m_nam->post(req, body.toJson());

    } else if (provider == "nvidia") {
        // NVIDIA NIM API (OpenAI-compatible)
        QString firstImg = base64Images.isEmpty() ? "" : base64Images.first();
        QJsonDocument body = buildOpenAIRequest(sysPrompt, userText, firstImg,
                                                model.isEmpty() ? "meta/llama-3.2-11b-vision-instruct" : model,
                                                true);
        QNetworkRequest req(QUrl("https://integrate.api.nvidia.com/v1/chat/completions"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

        m_currentReply = m_nam->post(req, body.toJson());

    } else {
        // OpenAI or custom base URL (including Cloudflare Workers proxies)
        QString m = model.isEmpty() ? "gpt-5.5-mini" : model;
        QString firstImg = base64Images.isEmpty() ? "" : base64Images.first();

        QString baseUrl = customBaseUrl.trimmed();
        if (baseUrl.isEmpty()) {
            baseUrl = cfg.currentApiBaseUrl().trimmed();
        }
        if (baseUrl.isEmpty()) {
            baseUrl = "https://api.openai.com/v1";
        }
        if (baseUrl.endsWith("/")) {
            baseUrl.chop(1);
        }

        // Detect Cloudflare Workers AI by base URL or model name prefix
        bool isCloudflare = baseUrl.contains("workers.dev") ||
                            baseUrl.contains("cf.ai") ||
                            baseUrl.contains("cloudflare.com") ||
                            m.startsWith("@cf/");

        QJsonDocument body;
        if (isCloudflare && !firstImg.isEmpty()) {
            // Cloudflare Workers AI uses different image format
            body = buildCloudflareRequest(sysPrompt, userText, firstImg, m);
        } else {
            body = buildOpenAIRequest(sysPrompt, userText, firstImg, m, true);
        }

        QNetworkRequest req(QUrl(baseUrl + "/chat/completions"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

        m_currentReply = m_nam->post(req, body.toJson());
    }
    // Use shorter timeout for non-streaming Cloudflare requests (they don't stream)
    bool isCloudflareImage = !screenshots.isEmpty() &&
        (cfg.currentApiBaseUrl().contains("workers.dev") ||
         cfg.currentApiBaseUrl().contains("cf.ai") ||
         cfg.currentApiBaseUrl().contains("cloudflare.com") ||
         customBaseUrl.contains("cloudflare.com") ||
         model.startsWith("@cf/"));
    int timeoutMs = isCloudflareImage ? 20000 : 45000;
    m_timeoutTimer->start(timeoutMs);
    connect(m_currentReply, &QNetworkReply::readyRead,  this, &AIManager::onReadyRead);
    connect(m_currentReply, &QNetworkReply::finished,   this, &AIManager::onReplyFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &AIManager::onError);
}

void AIManager::onReadyRead() {
    m_timeoutTimer->start(45000); // Reset timeout on data arrival
    if (!m_currentReply) return;

    QByteArray data = m_currentReply->readAll();
    m_buffer += QString::fromUtf8(data);

    // Memory Guard: Prevent buffer from growing indefinitely
    if (m_buffer.length() > 2 * 1024 * 1024) { // 2MB limit
        m_buffer.clear();
    }

    // Auto-detect stream protocol:
    // SSE stream uses "data: {" or "data: ["
    // NDJSON / OpenAI stream has lines with "choices"
    // Gemini direct stream has "candidates"
    bool isSseOrOpenAI = m_buffer.contains("data: ") || m_buffer.contains("\"choices\":") ||
                         (m_activeProvider != "gemini" && !m_buffer.contains("\"candidates\":"));

    if (isSseOrOpenAI) {
        // SSE / NDJSON / OpenAI format
        QStringList lines = m_buffer.split('\n');
        // If the buffer did not end with '\n', the last line may be incomplete - preserve it in m_buffer
        if (!m_buffer.endsWith('\n')) {
            m_buffer = lines.takeLast();
        } else {
            m_buffer.clear();
        }

        for (const QString& rawLine : lines) {
            QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith(':') || line == "data: [DONE]") continue;

            QString jsonStr = line;
            if (line.startsWith("data: ")) {
                jsonStr = line.mid(6).trimmed();
            }
            if (jsonStr.isEmpty()) continue;

            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();

                // Check for API error
                if (obj.contains("error") && obj["error"].isObject()) {
                    QString errMsg = obj["error"].toObject()["message"].toString();
                    if (!errMsg.isEmpty()) {
                        emit errorOccurred("API Error: " + errMsg);
                        if (m_currentReply) m_currentReply->abort();
                        return;
                    }
                }
                if (obj.contains("errors") && obj["errors"].isArray()) {
                    QJsonArray errs = obj["errors"].toArray();
                    if (!errs.isEmpty() && errs[0].isObject()) {
                        QString errMsg = errs[0].toObject()["message"].toString();
                        if (!errMsg.isEmpty()) {
                            emit errorOccurred("API Error: " + errMsg);
                            if (m_currentReply) m_currentReply->abort();
                            return;
                        }
                    }
                }

                // 1. OpenAI / Cloudflare choices delta or message
                if (obj.contains("choices")) {
                    QJsonArray choices = obj["choices"].toArray();
                    if (!choices.isEmpty()) {
                        QJsonObject choice0 = choices[0].toObject();
                        QString text;
                        if (choice0.contains("delta")) {
                            text = choice0["delta"].toObject()["content"].toString();
                        } else if (choice0.contains("message")) {
                            text = choice0["message"].toObject()["content"].toString();
                        }
                        if (!text.isEmpty()) {
                            m_fullResponse += text;
                            emit responseChunk(text);
                        }
                    }
                }
                // 2. Proxied Gemini candidates in SSE line
                else if (obj.contains("candidates")) {
                    QJsonArray candidates = obj["candidates"].toArray();
                    if (!candidates.isEmpty()) {
                        QJsonObject candidate = candidates[0].toObject();
                        QJsonObject content = candidate["content"].toObject();
                        QJsonArray parts = content["parts"].toArray();
                        for (int p = 0; p < parts.size(); ++p) {
                            QString text = parts[p].toObject()["text"].toString();
                            if (!text.isEmpty()) {
                                m_fullResponse += text;
                                emit responseChunk(text);
                            }
                        }
                    }
                }
                // 3. Cloudflare native format: {"result": {"response": "..."}}
                else if (obj.contains("result") && obj["result"].isObject()) {
                    QString text = obj["result"].toObject()["response"].toString();
                    if (!text.isEmpty()) {
                        m_fullResponse += text;
                        emit responseChunk(text);
                    }
                }
            }
        }
    } else {
        // Direct Gemini stream parser: objects delimited by '{' and '}'
        int start = m_buffer.indexOf('{');
        while (start != -1) {
            int braceCount = 0;
            int end = -1;
            bool inString = false;

            for (int i = start; i < m_buffer.length(); ++i) {
                QChar c = m_buffer[i];
                if (c == '"' && (i == 0 || m_buffer[i-1] != '\\')) {
                    inString = !inString;
                }
                if (!inString) {
                    if (c == '{') braceCount++;
                    else if (c == '}') {
                        braceCount--;
                        if (braceCount == 0) {
                            end = i;
                            break;
                        }
                    }
                }
            }

            if (end != -1) {
                QString jsonStr = m_buffer.mid(start, end - start + 1);
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QJsonArray candidates = obj["candidates"].toArray();
                    if (!candidates.isEmpty()) {
                        QJsonObject candidate = candidates[0].toObject();
                        QJsonObject content = candidate["content"].toObject();
                        QJsonArray parts = content["parts"].toArray();
                        for (int p = 0; p < parts.size(); ++p) {
                            QString text = parts[p].toObject()["text"].toString();
                            if (!text.isEmpty()) {
                                m_fullResponse += text;
                                emit responseChunk(text);
                            }
                        }
                    }
                }
                m_buffer = m_buffer.mid(end + 1);
                while (!m_buffer.isEmpty() && (m_buffer[0] == ',' || m_buffer[0] == '\r' || m_buffer[0] == '\n' || m_buffer[0] == '[' || m_buffer[0] == ']' || m_buffer[0] == ' ')) {
                    m_buffer = m_buffer.mid(1);
                }
                start = m_buffer.indexOf('{');
            } else {
                break;
            }
        }
    }
}

void AIManager::onReplyFinished() {
    m_timeoutTimer->stop();
    m_busy = false;
    if (m_currentReply) {
        // Read any remaining trailing data from socket
        QByteArray remaining = m_currentReply->readAll();
        if (!remaining.isEmpty()) {
            m_buffer += QString::fromUtf8(remaining);
        }

        bool hasError = (m_currentReply->error() != QNetworkReply::NoError
                         && m_currentReply->error() != QNetworkReply::RemoteHostClosedError);

        // Process any leftover buffered data
        QString rawResponse = m_buffer.trimmed();
        if (!rawResponse.isEmpty()) {
            // First check if it's SSE lines or NDJSON
            QStringList lines = rawResponse.split('\n', Qt::SkipEmptyParts);
            for (const QString& rawLine : lines) {
                QString line = rawLine.trimmed();
                if (line.isEmpty() || line.startsWith(':') || line == "data: [DONE]") continue;
                if (line.startsWith("data: ")) line = line.mid(6).trimmed();
                if (line.isEmpty()) continue;

                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    if (obj.contains("error") && obj["error"].isObject()) {
                        QString errMsg = obj["error"].toObject()["message"].toString();
                        if (!errMsg.isEmpty()) {
                            emit errorOccurred("API Error: " + errMsg);
                            m_currentReply->deleteLater();
                            m_currentReply = nullptr;
                            return;
                        }
                    }
                    if (obj.contains("choices")) {
                        QJsonArray choices = obj["choices"].toArray();
                        if (!choices.isEmpty()) {
                            QJsonObject c = choices[0].toObject();
                            QString token;
                            if (c.contains("delta")) token = c["delta"].toObject()["content"].toString();
                            else if (c.contains("message")) token = c["message"].toObject()["content"].toString();
                            if (!token.isEmpty()) {
                                m_fullResponse += token;
                            }
                        }
                    } else if (obj.contains("candidates")) {
                        QJsonArray candidates = obj["candidates"].toArray();
                        if (!candidates.isEmpty()) {
                            QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
                            for (int p = 0; p < parts.size(); ++p) {
                                m_fullResponse += parts[p].toObject()["text"].toString();
                            }
                        }
                    } else if (obj.contains("result") && obj["result"].isObject()) {
                        m_fullResponse += obj["result"].toObject()["response"].toString();
                    }
                }
            }

            // Also check if entire rawResponse is a single JSON document (non-streaming standard payload)
            if (m_fullResponse.isEmpty()) {
                QJsonDocument fullDoc = QJsonDocument::fromJson(rawResponse.toUtf8());
                if (!fullDoc.isNull() && fullDoc.isObject()) {
                    QJsonObject obj = fullDoc.object();
                    if (obj.contains("choices")) {
                        QJsonArray choices = obj["choices"].toArray();
                        if (!choices.isEmpty()) {
                            QJsonObject c = choices[0].toObject();
                            QString token;
                            if (c.contains("message")) token = c["message"].toObject()["content"].toString();
                            else if (c.contains("delta")) token = c["delta"].toObject()["content"].toString();
                            if (!token.isEmpty()) m_fullResponse = token;
                        }
                    } else if (obj.contains("candidates")) {
                        QJsonArray candidates = obj["candidates"].toArray();
                        if (!candidates.isEmpty()) {
                            QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
                            for (int p = 0; p < parts.size(); ++p) {
                                m_fullResponse += parts[p].toObject()["text"].toString();
                            }
                        }
                    } else if (obj.contains("result") && obj["result"].isObject()) {
                        m_fullResponse = obj["result"].toObject()["response"].toString();
                    }
                }
            }
        }

        m_buffer.clear();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        if (!hasError) {
            if (m_fullResponse.trimmed().isEmpty()) {
                QString raw = rawResponse.trimmed();
                QString cleanErr;
                QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    if (obj.contains("message") && obj["message"].isString()) {
                        cleanErr = obj["message"].toString();
                    } else if (obj.contains("error") && obj["error"].isObject()) {
                        cleanErr = obj["error"].toObject()["message"].toString();
                    } else if (obj.contains("error") && obj["error"].isString()) {
                        cleanErr = obj["error"].toString();
                    }
                }
                if (!cleanErr.isEmpty()) {
                    emit errorOccurred("AI Provider Error: " + cleanErr.left(200));
                } else if (raw.contains("error") || raw.contains("Error") || raw.contains("403") || raw.contains("401")) {
                    emit errorOccurred("AI Provider Error: " + raw.left(200));
                } else if (!raw.isEmpty()) {
                    emit errorOccurred("AI Provider Error: " + raw.left(200));
                } else {
                    emit errorOccurred("Empty response from AI provider. Check your model settings.");
                }
            } else {
                emit responseComplete(m_fullResponse);
            }
        }
    } else {
        // currentReply already cleaned up by onError — don't emit responseComplete
    }
}

void AIManager::onError(QNetworkReply::NetworkError code) {
    m_timeoutTimer->stop();

    // RemoteHostClosedError = server closed the stream after sending everything.
    // This is NORMAL for streaming APIs (Groq, Gemini, OpenAI SSE).
    // If we already received response data, treat it as a successful completion.
    if (code == QNetworkReply::RemoteHostClosedError) {
        m_busy = false;
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        if (!m_fullResponse.isEmpty()) {
            // We got data before the close — treat as complete
            emit responseComplete(m_fullResponse);
        } else {
            emit errorOccurred("Connection closed before response. Please retry.");
        }
        return;
    }

    // OperationCanceledError = we aborted it ourselves (timeout/user action).
    // Don't show a confusing error to the user.
    if (code == QNetworkReply::OperationCanceledError) {
        m_busy = false;
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        if (!m_fullResponse.isEmpty()) {
            emit responseComplete(m_fullResponse); // show whatever we got
        } else {
            emit errorOccurred("Request timed out. Try again or check your connection.");
        }
        return;
    }

    // All other errors — show the HTTP code or a clean message
    QString errStr = m_currentReply ? m_currentReply->errorString() : "Network error";
    int httpCode = 0;
    QString rawResponse = m_buffer;
    if (m_currentReply) {
        httpCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        rawResponse += QString::fromUtf8(m_currentReply->readAll());
    }

    QString detailedError;
    rawResponse = rawResponse.trimmed();
    if (!rawResponse.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(rawResponse.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error") && obj["error"].isObject()) {
                detailedError = obj["error"].toObject()["message"].toString();
            } else if (obj.contains("errors") && obj["errors"].isArray()) {
                QJsonArray errs = obj["errors"].toArray();
                if (!errs.isEmpty() && errs[0].isObject()) {
                    detailedError = errs[0].toObject()["message"].toString();
                }
            } else if (obj.contains("message") && obj["message"].isString()) {
                QString m = obj["message"].toString();
                QJsonDocument nestedDoc = QJsonDocument::fromJson(m.toUtf8());
                if (!nestedDoc.isNull() && nestedDoc.isObject()) {
                    QJsonObject nestedObj = nestedDoc.object();
                    if (nestedObj.contains("errors") && nestedObj["errors"].isArray()) {
                        QJsonArray nestedErrs = nestedObj["errors"].toArray();
                        if (!nestedErrs.isEmpty() && nestedErrs[0].isObject()) {
                            detailedError = nestedErrs[0].toObject()["message"].toString();
                        }
                    } else if (nestedObj.contains("error") && nestedObj["error"].isObject()) {
                        detailedError = nestedObj["error"].toObject()["message"].toString();
                    }
                }
                if (detailedError.isEmpty()) {
                    detailedError = m;
                }
            }
        }
    }

    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    QString status;
    if (detailedError.contains("LICENSE_REVOKED", Qt::CaseInsensitive) || rawResponse.contains("LICENSE_REVOKED", Qt::CaseInsensitive)) {
        AccountManager::instance().revokeLocalPro();
        status = "🔒 Pro License Revoked or Inactive. Please renew your subscription in Settings to unlock AI solves.";
    } else if (detailedError.contains("AUTH_REQUIRED", Qt::CaseInsensitive) || rawResponse.contains("AUTH_REQUIRED", Qt::CaseInsensitive)) {
        status = "🔒 Please log in with Google (Ctrl+S or tray menu) to verify your account.";
    } else if (!detailedError.isEmpty()) {
        status = QString("API Error: %1").arg(detailedError);
    } else if (httpCode == 429) {
        status = "Rate limited (429). Wait a moment then retry, or switch API slot.";
    } else if (httpCode == 401 || httpCode == 403) {
        status = QString("Invalid API key or access denied (HTTP %1). Check Settings.").arg(httpCode);
    } else if (httpCode > 0) {
        status = QString("API error HTTP %1. Try again.").arg(httpCode);
    } else {
        status = "Network error. Check your internet connection and retry.";
    }
    emit errorOccurred(status);
    m_busy = false;
}

void AIManager::transcribeAudio(const QString& filePath, const QList<QPixmap>& screenshots) {
    if (m_currentReply) {
        m_currentReply->disconnect();
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    auto& cfg = AppConfig::instance();
    QString apiKey = cfg.currentApiKey();
    QString provider = cfg.currentApiProvider();
    
    if (apiKey.trimmed().isEmpty()) {
        if (cfg.isPro() || cfg.canUseFreeQuery()) {
            provider = cfg.proCloudProvider();
            apiKey = cfg.proCloudKey();
            if (apiKey.isEmpty()) {
                AccountManager::instance().fetchCloudConfig();
                emit errorOccurred("Connecting to Cloud Engine... Please try again.");
                return;
            }
        } else {
            emit errorOccurred("API key not set. Please configure it in Settings.");
            return;
        }
    }

    m_busy = true;
    m_fullResponse.clear();
    m_buffer.clear();
    emit requestStarted();

    // Cache screenshots for after transcription
    m_lastScreenshots = screenshots;

    QUrl url;
    QString model;
    if (provider == "groq") {
        url = QUrl("https://api.groq.com/openai/v1/audio/transcriptions");
        model = "whisper-large-v3";
    } else {
        // OpenAI default
        QString baseUrl = cfg.currentApiBaseUrl().trimmed();
        if (baseUrl.isEmpty()) {
            baseUrl = "https://api.openai.com/v1";
        }
        if (baseUrl.endsWith("/")) {
            baseUrl.chop(1);
        }
        url = QUrl(baseUrl + "/audio/transcriptions");
        model = "whisper-1";
    }

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"model\""));
    modelPart.setBody(model.toUtf8());
    multiPart->append(modelPart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("audio/wav"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(filePath).fileName()));
    
    QFile* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open audio file for transcription.");
        delete multiPart;
        delete file;
        m_busy = false;
        return;
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // Automatically deletes file when multiPart is deleted
    multiPart->append(filePart);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    // Post the multiPart
    m_currentReply = m_nam->post(req, multiPart);
    multiPart->setParent(m_currentReply); // Delete multiPart when reply is deleted

    m_timeoutTimer->start(45000); // 45s timeout

    connect(m_currentReply, &QNetworkReply::finished, this, [this, screenshots, filePath]() {
        m_timeoutTimer->stop();
        m_busy = false;
        if (!m_currentReply) {
            QFile::remove(filePath);
            return;
        }
        
        if (m_currentReply->error() != QNetworkReply::NoError) {
            QString err = m_currentReply->errorString();
            int httpCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
            QFile::remove(filePath);
            emit errorOccurred(QString("Transcription error: %1 (HTTP %2)").arg(err).arg(httpCode));
            return;
        }

        QByteArray resData = m_currentReply->readAll();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        QFile::remove(filePath);

        QJsonDocument doc = QJsonDocument::fromJson(resData);
        QString text = doc.object()["text"].toString();

        if (text.isEmpty()) {
            emit errorOccurred("Transcription returned empty text.");
            return;
        }

        // Debug output removed for stealth
        
        // Now call performRequest with this text!
        performRequest(screenshots, text);
    });
}

void AIManager::cancelTranscriptionOnly() {
    if (m_transcribing) {
        if (m_transcribeReply) {
            m_transcribeReply->disconnect();
            m_transcribeReply->abort();
            m_transcribeReply->deleteLater();
            m_transcribeReply = nullptr;
        }
        m_transcribing = false;
    }
}

void AIManager::transcribeAudioOnly(const QString& filePath) {
    cancelTranscriptionOnly();

    auto& cfg = AppConfig::instance();
    QString apiKey = cfg.currentApiKey();
    QString provider = cfg.currentApiProvider();
    
    if (apiKey.isEmpty()) {
        if (cfg.isPro() || cfg.canUseFreeQuery()) {
            provider = cfg.proCloudProvider();
            apiKey = cfg.proCloudKey();
            if (apiKey.isEmpty()) {
                AccountManager::instance().fetchCloudConfig();
                QFile::remove(filePath);
                return;
            }
        } else {
            QFile::remove(filePath);
            return;
        }
    }

    m_transcribing = true;

    if (provider == "gemini") {
        // Read file to base64
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QFile::remove(filePath);
            m_transcribing = false;
            return;
        }
        QByteArray audioData = file.readAll();
        file.close();
        QFile::remove(filePath); // clean up immediately since we read it

        if (audioData.isEmpty()) {
            m_transcribing = false;
            return;
        }

        QString base64Audio = QString::fromUtf8(audioData.toBase64());
        QString model = cfg.currentApiModel();
        if (model.isEmpty()) model = "gemini-2.5-flash";

        QString endpoint = QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2")
                               .arg(model)
                               .arg(apiKey);

        // Prompt Gemini to only transcribe
        QJsonDocument body = buildGeminiRequest(
            "You are a verbatim speech transcription tool. Output ONLY the text you hear in the audio file. Do not summarize, do not solve any questions, do not add explanations. If there is no speech, output nothing.",
            "Transcribe verbatim:",
            QStringList(),
            base64Audio,
            "audio/wav"
        );

        QUrl url(endpoint);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        m_transcribeReply = m_nam->post(req, body.toJson());
        
        connect(m_transcribeReply, &QNetworkReply::finished, this, [this]() {
            m_transcribing = false;
            if (!m_transcribeReply) return;
            
            if (m_transcribeReply->error() == QNetworkReply::NoError) {
                QByteArray resData = m_transcribeReply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(resData);
                QJsonObject obj = doc.object();
                QJsonArray candidates = obj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject content = candidates[0].toObject()["content"].toObject();
                    QJsonArray parts = content["parts"].toArray();
                    QString text;
                    for (int p = 0; p < parts.size(); ++p) {
                        text += parts[p].toObject()["text"].toString();
                    }
                    emit transcriptionOnlyFinished(text.trimmed());
                }
            }
            m_transcribeReply->deleteLater();
            m_transcribeReply = nullptr;
        });

    } else {
        // OpenAI / Groq: use Whisper
        QUrl url;
        QString model;
        if (provider == "groq") {
            url = QUrl("https://api.groq.com/openai/v1/audio/transcriptions");
            model = "whisper-large-v3";
        } else {
            QString baseUrl = cfg.currentApiBaseUrl().trimmed();
            if (baseUrl.isEmpty()) baseUrl = "https://api.openai.com/v1";
            if (baseUrl.endsWith("/")) baseUrl.chop(1);
            url = QUrl(baseUrl + "/audio/transcriptions");
            model = "whisper-1";
        }

        QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart modelPart;
        modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"model\""));
        modelPart.setBody(model.toUtf8());
        multiPart->append(modelPart);

        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("audio/wav"));
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(filePath).fileName()));
        
        QFile* file = new QFile(filePath);
        if (!file->open(QIODevice::ReadOnly)) {
            delete multiPart;
            delete file;
            QFile::remove(filePath);
            m_transcribing = false;
            return;
        }
        filePart.setBodyDevice(file);
        file->setParent(multiPart);
        multiPart->append(filePart);

        QNetworkRequest req(url);
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

        m_transcribeReply = m_nam->post(req, multiPart);
        multiPart->setParent(m_transcribeReply);

        connect(m_transcribeReply, &QNetworkReply::finished, this, [this, filePath]() {
            m_transcribing = false;
            QFile::remove(filePath);
            if (!m_transcribeReply) return;

            if (m_transcribeReply->error() == QNetworkReply::NoError) {
                QByteArray resData = m_transcribeReply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(resData);
                QString text = doc.object()["text"].toString();
                emit transcriptionOnlyFinished(text.trimmed());
            }
            m_transcribeReply->deleteLater();
            m_transcribeReply = nullptr;
        });
    }
}
