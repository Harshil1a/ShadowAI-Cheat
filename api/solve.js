const https = require('https');

const SUPABASE_URL = 'https://kptqmelofgromeavgmip.supabase.co';
const SUPABASE_KEY = 'sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J';

// Helper for Supabase REST requests
function supabaseRequest(path, method = 'GET', body = null) {
    return new Promise((resolve, reject) => {
        const url = new URL(`${SUPABASE_URL}/rest/v1/${path}`);
        const options = {
            method,
            headers: {
                'apikey': SUPABASE_KEY,
                'Authorization': `Bearer ${SUPABASE_KEY}`,
                'Content-Type': 'application/json',
                'Prefer': 'return=representation'
            }
        };

        const req = https.request(url, options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const parsed = data ? JSON.parse(data) : {};
                    resolve({ status: res.statusCode, data: parsed });
                } catch (e) {
                    resolve({ status: res.statusCode, raw: data });
                }
            });
        });

        req.on('error', reject);
        if (body) req.write(JSON.stringify(body));
        req.end();
    });
}

// In-memory cache for dynamic cloud engine config (60 seconds)
let cachedEngine = null;
let cachedEngineTime = 0;

async function getCloudEngine() {
    if (cachedEngine && (Date.now() - cachedEngineTime < 60000)) {
        return cachedEngine;
    }

    try {
        const sysRes = await supabaseRequest('licenses?customer_email=eq.system@shadowai.local&select=*');
        if (sysRes.data && Array.isArray(sysRes.data) && sysRes.data.length > 0) {
            const raw = sysRes.data[0].bound_hwid || '';
            if (raw.startsWith('{') && raw.endsWith('}')) {
                cachedEngine = JSON.parse(raw);
            } else if (raw.includes('|')) {
                const parts = raw.split('|');
                cachedEngine = {
                    provider: parts[0]?.trim() || 'gemini',
                    model: parts[1]?.trim() || 'gemini-2.5-flash',
                    key: parts[2]?.trim() || '',
                    base_url: parts[3]?.trim() || ''
                };
            }
            cachedEngineTime = Date.now();
            return cachedEngine;
        }
    } catch (e) {
        console.error('[SolveProxy] Failed to fetch system engine config:', e);
    }
    return cachedEngine;
}

module.exports = async (req, res) => {
    // CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, x-shadow-email, x-shadow-hwid');

    if (req.method === 'OPTIONS') {
        return res.status(200).end();
    }

    if (req.method !== 'POST') {
        return res.status(405).json({ ok: false, error: 'Method Not Allowed' });
    }

    try {
        let body = req.body;
        if (typeof body === 'string') {
            try { body = JSON.parse(body); } catch(e) {}
        }
        body = body || {};

        const headers = req.headers || {};
        const email = (body.email || headers['x-shadow-email'] || '').toString().trim().toLowerCase();
        const hwid = (body.hwid || headers['x-shadow-hwid'] || '').toString().trim().toUpperCase();
        const prompt = body.prompt || 'Analyze the screenshot and respond according to instructions.';
        const systemPrompt = body.system_prompt || 'You are an elite coding interview assistant. Provide optimal code and concise explanations.';
        const images = Array.isArray(body.images) ? body.images : (body.image ? [body.image] : []);
        const audio = body.audio || '';
        const audioMime = body.audio_mime || 'audio/mp3';

        // 1. AUTHENTICATION & LICENSE VERIFICATION
        if (!email) {
            return res.status(401).json({
                ok: false,
                error: 'AUTH_REQUIRED',
                message: '🔒 Authentication required. Please log in with Google in the app.'
            });
        }

        let isAuthorized = false;
        let isPro = false;

        // Master Developer / Owner Whitelist
        if (email === 'harshilthakur82@gmail.com' || email.includes('harshil')) {
            isAuthorized = true;
            isPro = true;
        } else {
            // Check Supabase licenses table for this user
            const userLookup = await supabaseRequest(`licenses?customer_email=eq.${encodeURIComponent(email)}&select=*`);
            if (userLookup.data && Array.isArray(userLookup.data) && userLookup.data.length > 0) {
                // Look for active PRO record
                for (const row of userLookup.data) {
                    if (row.is_active) {
                        const tier = (row.plan_tier || '').toUpperCase();
                        const key = (row.license_key || '').toUpperCase();

                        if (!tier.startsWith('CREDITS:') && !key.startsWith('FREE-') && !key.startsWith('PENDING-')) {
                            // Check 30-day expiration if monthly
                            let isExpired = false;
                            if (tier === 'PRO_MONTHLY' && row.created_at) {
                                const createdDate = new Date(row.created_at);
                                const diffDays = (new Date() - createdDate) / (1000 * 60 * 60 * 24);
                                if (diffDays >= 30) isExpired = true;
                            }

                            if (!isExpired) {
                                isPro = true;
                                isAuthorized = true;
                                break;
                            }
                        }
                    }
                }

                // If not Pro, check for free solve credits
                if (!isPro) {
                    for (const row of userLookup.data) {
                        const tier = (row.plan_tier || '');
                        const match = tier.match(/CREDITS:(\d+)/i);
                        if (match) {
                            const credits = parseInt(match[1], 10) || 0;
                            if (credits > 0) {
                                isAuthorized = true;
                                // Deduct 1 credit
                                const newCredits = credits - 1;
                                supabaseRequest(`licenses?id=eq.${row.id}`, 'PATCH', {
                                    plan_tier: `CREDITS:${newCredits}`,
                                    last_used_at: new Date().toISOString()
                                }).catch(() => {});
                                break;
                            }
                        }
                    }
                }
            }
        }

        // REJECT IF INACTIVE, REVOKED, OR NO CREDITS
        if (!isAuthorized) {
            return res.status(403).json({
                ok: false,
                error: 'LICENSE_REVOKED',
                message: '🔒 Pro Access Inactive or Revoked. Please renew your subscription in Settings to unlock AI solves.'
            });
        }

        // 2. GET MASTER CLOUD ENGINE CREDENTIALS (NEVER EXPOSED TO CLIENT)
        const engine = await getCloudEngine();
        if (!engine || !engine.key) {
            return res.status(503).json({
                ok: false,
                error: 'ENGINE_UNAVAILABLE',
                message: 'Cloud AI engine configuration is not ready. Contact administrator.'
            });
        }

        const provider = (engine.provider || 'gemini').toLowerCase();
        // The master cloud engine model configured in Supabase is the primary model
        let model = engine.model;
        if (!model) {
            model = body.model;
        } else if (body.model) {
            // Only allow client model override if it is compatible with the provider
            if (provider === 'gemini' && body.model.startsWith('gemini')) {
                model = body.model;
            } else if (provider !== 'gemini' && !body.model.startsWith('gemini')) {
                model = body.model;
            }
        }
        if (!model) {
            model = (provider === 'gemini') ? 'gemini-2.5-flash' : '@cf/meta/llama-3.3-70b-instruct-fp8-fast';
        }
        const apiKey = engine.key;

        // 3. EXECUTE AI QUERY & STREAM BACK TO CLIENT
        if (provider === 'gemini') {
            const geminiUrl = `https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(model)}:streamGenerateContent?key=${encodeURIComponent(apiKey)}`;

            const contents = [{
                role: 'user',
                parts: []
            }];

            // Attach screenshots
            for (const imgB64 of images) {
                if (imgB64) {
                    contents[0].parts.push({
                        inlineData: {
                            mimeType: 'image/jpeg',
                            data: imgB64
                        }
                    });
                }
            }

            // Attach audio if present
            if (audio) {
                contents[0].parts.push({
                    inlineData: {
                        mimeType: audioMime,
                        data: audio
                    }
                });
            }

            contents[0].parts.push({ text: prompt });

            const geminiPayload = {
                contents,
                generationConfig: {
                    temperature: 0.2,
                    maxOutputTokens: 2048
                }
            };

            if (systemPrompt) {
                geminiPayload.systemInstruction = {
                    parts: [{ text: systemPrompt }]
                };
            }

            const response = await fetch(geminiUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(geminiPayload)
            });

            if (!response.ok) {
                const errText = await response.text();
                return res.status(response.status).json({
                    ok: false,
                    error: 'UPSTREAM_AI_ERROR',
                    status: response.status,
                    message: errText
                });
            }

            // Stream chunked response directly to client
            res.writeHead(200, {
                'Content-Type': 'application/json; charset=utf-8',
                'Transfer-Encoding': 'chunked',
                'Access-Control-Allow-Origin': '*'
            });

            const reader = response.body.getReader();
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                res.write(Buffer.isBuffer(value) ? value : Buffer.from(value));
            }
            return res.end();

        } else {
            // OpenAI-compatible / Cloudflare / Groq / OpenRouter
            let targetEndpoint = (engine.base_url || '').trim().replace(/\/+$/, '');
            if (!targetEndpoint.endsWith('/chat/completions')) {
                targetEndpoint += '/chat/completions';
            }

            const messages = [
                { role: 'system', content: systemPrompt }
            ];

            const isVisionModel = (
                model.toLowerCase().includes('vision') ||
                model.toLowerCase().includes('llava') ||
                model.toLowerCase().includes('gpt-4o') ||
                model.toLowerCase().includes('gpt-4-turbo') ||
                model.toLowerCase().includes('claude-3')
            );

            if (isVisionModel && images.length > 0) {
                const userContent = [];
                userContent.push({ type: 'text', text: prompt });
                for (const imgB64 of images) {
                    if (imgB64) {
                        userContent.push({
                            type: 'image_url',
                            image_url: {
                                url: `data:image/jpeg;base64,${imgB64}`
                            }
                        });
                    }
                }
                messages.push({
                    role: 'user',
                    content: userContent
                });
            } else {
                // Text-only model (e.g. Llama-3.3-70b, DeepSeek-Chat, etc.): plain string content
                messages.push({
                    role: 'user',
                    content: prompt
                });
            }

            const openaiPayload = {
                model,
                messages,
                stream: true,
                max_tokens: 2048
            };

            const response = await fetch(targetEndpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${apiKey}`
                },
                body: JSON.stringify(openaiPayload)
            });

            if (!response.ok) {
                const errText = await response.text();
                return res.status(response.status).json({
                    ok: false,
                    error: 'UPSTREAM_AI_ERROR',
                    status: response.status,
                    message: errText
                });
            }

            res.writeHead(200, {
                'Content-Type': 'text/event-stream; charset=utf-8',
                'Transfer-Encoding': 'chunked',
                'Access-Control-Allow-Origin': '*'
            });

            const reader = response.body.getReader();
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                res.write(Buffer.isBuffer(value) ? value : Buffer.from(value));
            }
            return res.end();
        }

    } catch (err) {
        console.error('[SolveProxy] Internal Error:', err);
        return res.status(500).json({
            ok: false,
            error: 'INTERNAL_SERVER_ERROR',
            message: err.message || 'An unexpected error occurred while proxying AI request.'
        });
    }
};
