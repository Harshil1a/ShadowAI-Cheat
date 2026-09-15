module.exports = async (req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

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
        const { provider, key, model, base_url } = body || {};

        if (!key) {
            return res.status(400).json({ ok: false, error: 'API key / token is required' });
        }

        const t0 = Date.now();

        if (provider === 'gemini') {
            const m = model || 'gemini-2.5-flash';
            const targetUrl = `https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(m)}:generateContent?key=${encodeURIComponent(key)}`;
            
            const response = await fetch(targetUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    contents: [{ parts: [{ text: 'Say OK' }] }]
                })
            });

            const elapsed = Date.now() - t0;
            const data = await response.json();

            if (response.ok) {
                return res.status(200).json({
                    ok: true,
                    elapsedMs: elapsed,
                    status: response.status,
                    provider: 'gemini',
                    model: m,
                    response: data.candidates?.[0]?.content?.parts?.[0]?.text?.trim() || 'OK'
                });
            } else {
                return res.status(200).json({
                    ok: false,
                    elapsedMs: elapsed,
                    status: response.status,
                    error: data.error?.message || `HTTP ${response.status}`
                });
            }
        } else {
            // Cloudflare, DeepSeek, Groq, or OpenAI-compatible
            let targetEndpoint = (base_url || '').trim().replace(/\/+$/, '');
            if (!targetEndpoint.endsWith('/chat/completions')) {
                targetEndpoint += '/chat/completions';
            }

            const m = model || '@cf/meta/llama-3.3-70b-instruct-fp8-fast';

            const response = await fetch(targetEndpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${key.trim()}`
                },
                body: JSON.stringify({
                    model: m,
                    messages: [{ role: 'user', content: 'Say OK' }],
                    max_tokens: 10
                })
            });

            const elapsed = Date.now() - t0;
            const text = await response.text();

            let data;
            try {
                data = JSON.parse(text);
            } catch(e) {
                data = null;
            }

            if (response.ok) {
                const replyText = data?.choices?.[0]?.message?.content?.trim() || 'OK';
                return res.status(200).json({
                    ok: true,
                    elapsedMs: elapsed,
                    status: response.status,
                    provider: provider || 'cloudflare',
                    model: m,
                    response: replyText
                });
            } else {
                const errMsg = data?.error?.message || data?.errors?.[0]?.message || text.substring(0, 200) || `HTTP ${response.status}`;
                return res.status(200).json({
                    ok: false,
                    elapsedMs: elapsed,
                    status: response.status,
                    error: errMsg
                });
            }
        }
    } catch (err) {
        return res.status(500).json({
            ok: false,
            error: err.message || 'Internal proxy connection error'
        });
    }
};
