const https = require('https');

const SUPABASE_URL = 'https://kptqmelofgromeavgmip.supabase.co';
const SUPABASE_KEY = 'sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J';

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

module.exports = async (req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
        return res.status(200).end();
    }

    try {
        const query = req.query || {};
        const hwid = (query.hwid || (req.body && req.body.hwid) || '').toString().trim().toUpperCase();
        const action = (query.action || (req.body && req.body.action) || 'balance').toString().toLowerCase();

        if (!hwid) {
            return res.status(400).json({ error: 'Missing hwid parameter' });
        }

        // Look up license / credit record by bound_hwid
        const lookup = await supabaseRequest(`licenses?bound_hwid=eq.${encodeURIComponent(hwid)}&select=*`);
        let credits = 0;
        let isPro = false;
        let recordId = null;

        if (lookup.data && Array.isArray(lookup.data) && lookup.data.length > 0) {
            const row = lookup.data[0];
            recordId = row.id;

            if (row.is_active && row.plan_tier && row.plan_tier.startsWith('PRO_')) {
                isPro = true;
                credits = 999999;
            } else {
                const match = (row.plan_tier || '').match(/CREDITS:(\d+)/i);
                if (match) {
                    credits = parseInt(match[1], 10) || 0;
                }
            }
        }

        // Handle consume action (when user takes a solve)
        if (action === 'consume') {
            if (isPro) {
                return res.status(200).json({
                    success: true,
                    is_pro: true,
                    remaining_credits: 999999,
                    message: 'Pro user: unlimited solves'
                });
            }

            if (credits <= 0) {
                return res.status(403).json({
                    success: false,
                    is_pro: false,
                    remaining_credits: 0,
                    error: 'NO_CREDITS',
                    message: '0 credits remaining. Complete a sponsor ad to earn more solves.'
                });
            }

            const newCredits = Math.max(0, credits - 1);

            if (recordId) {
                await supabaseRequest(`licenses?id=eq.${recordId}`, 'PATCH', {
                    plan_tier: `CREDITS:${newCredits}`,
                    last_used_at: new Date().toISOString()
                });
            }

            return res.status(200).json({
                success: true,
                is_pro: false,
                remaining_credits: newCredits,
                message: '1 credit consumed'
            });
        }

        // Default: return balance
        return res.status(200).json({
            success: true,
            hwid,
            is_pro: isPro,
            credits,
            ad_url: `https://loot-link.com/s?bz4nCWsI&puid=${encodeURIComponent(hwid)}`
        });
    } catch (err) {
        console.error('[User Credits Error]', err);
        return res.status(500).json({ error: 'Internal server error', details: err.message });
    }
};
