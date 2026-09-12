const https = require('https');

const SUPABASE_URL = 'https://kptqmelofgromeavgmip.supabase.co';
const SUPABASE_KEY = 'sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J';

// Helper to make Supabase REST requests
function supabaseRequest(path, method = 'GET', body = null) {
    return new Promise((resolve, reject) => {
        const url = new URL(`${SUPABASE_URL}/rest/v1/${path}`);
        const options = {
            method,
            headers: {
                'apikey': SUPABASE_KEY,
                'Authorization': `Bearer ${SUPABASE_KEY}`,
                'Content-Type': 'application/json',
                'Prefer': method === 'POST' ? 'return=representation' : 'return=representation'
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
    // CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
        return res.status(200).end();
    }

    try {
        // Query parameters sent by LootLabs
        // Example: /api/lootlabs-postback?click_id=HWID&ip=1.2.3.4&unique_id=TX123
        const query = req.query || {};
        const clickId = (query.click_id || query.puid || '').toString().trim().toUpperCase();
        const ip = (query.ip || req.headers['x-forwarded-for'] || '').toString();
        const uniqueId = (query.unique_id || '').toString();

        if (!clickId) {
            console.warn('[LootLabs Webhook] Missing click_id (HWID).');
            return res.status(400).json({ error: 'Missing click_id parameter' });
        }

        console.log(`[LootLabs Webhook] Verified task for HWID: ${clickId} | UniqueID: ${uniqueId} | IP: ${ip}`);

        // 1. Query existing license / credit record for this HWID
        const lookup = await supabaseRequest(`licenses?bound_hwid=eq.${encodeURIComponent(clickId)}&select=*`);
        let currentCredits = 0;
        let recordId = null;

        if (lookup.data && Array.isArray(lookup.data) && lookup.data.length > 0) {
            const row = lookup.data[0];
            recordId = row.id;

            // If user is already an active PRO subscriber, keep PRO intact
            if (row.is_active && row.plan_tier && row.plan_tier.startsWith('PRO_')) {
                return res.status(200).send('OK - PRO_USER_ACTIVE');
            }

            const match = (row.plan_tier || '').match(/CREDITS:(\d+)/i);
            if (match) {
                currentCredits = parseInt(match[1], 10) || 0;
            }
        }

        const newCredits = currentCredits + 1; // 1 Task = +1 Credit

        if (recordId) {
            // Update existing record
            await supabaseRequest(`licenses?id=eq.${recordId}`, 'PATCH', {
                plan_tier: `CREDITS:${newCredits}`,
                is_active: true,
                last_used_at: new Date().toISOString()
            });
        } else {
            // Create new record for this machine
            await supabaseRequest('licenses', 'POST', {
                license_key: `FREE-CREDITS-${clickId}`,
                customer_email: `${clickId.toLowerCase()}@free.shadowai`,
                plan_tier: `CREDITS:${newCredits}`,
                is_active: true,
                bound_hwid: clickId,
                last_used_at: new Date().toISOString()
            });
        }

        console.log(`[LootLabs Webhook] Successfully credited HWID: ${clickId} -> New Balance: ${newCredits}`);

        // Respond 200 OK so LootLabs logs the conversion as confirmed
        return res.status(200).json({
            status: 'success',
            hwid: clickId,
            credits_awarded: 1,
            total_credits: newCredits,
            unique_id: uniqueId
        });
    } catch (err) {
        console.error('[LootLabs Webhook Error]', err);
        return res.status(500).json({ error: 'Internal server error', details: err.message });
    }
};
