/**
 * SHADOW AI // CYBERPUNK GEO-LOCATION RADAR MAP & TELEMETRY CONTROLLER
 */

(function () {
  'use strict';

  // ── Supabase Cloud Database Configuration ──────────────────────────────────
  const SUPABASE_URL = 'https://kptqmelofgromeavgmip.supabase.co';
  const SUPABASE_KEY = 'sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J';

  // ── Global Tech Hubs ────────────────────────────────────────────────────────
  const GLOBAL_HUBS = [
    { name: 'SILICON_VALLEY', coords: [-122.4194, 37.7749], ping: '16ms' },
    { name: 'LONDON_NODE',    coords: [-0.1276, 51.5074],   ping: '22ms' },
    { name: 'FRANKFURT_HUB',  coords: [8.6821, 50.1109],    ping: '24ms' },
    { name: 'TOKYO_RELAY',    coords: [139.6917, 35.6895],  ping: '31ms' },
    { name: 'SINGAPORE_CORE', coords: [103.8198, 1.3521],   ping: '28ms' },
    { name: 'SAO_PAULO_NODE', coords: [-46.6333, -23.5505], ping: '45ms' },
    { name: 'SYDNEY_STATION', coords: [151.2093, -33.8688], ping: '52ms' },
    { name: 'DELHI_GATEWAY',  coords: [77.2090, 28.6139],   ping: '19ms' }
  ];

  // Global hub connection routes (Pairs)
  const HUB_CONNECTIONS = [
    [0, 1], // SF <-> London
    [1, 2], // London <-> Frankfurt
    [2, 3], // Frankfurt <-> Tokyo
    [3, 4], // Tokyo <-> Singapore
    [4, 7], // Singapore <-> Delhi
    [7, 2], // Delhi <-> Frankfurt
    [0, 5], // SF <-> Sao Paulo
    [4, 6]  // Singapore <-> Sydney
  ];

  const svg = d3.select('#world-map-svg');
  const width = 960;
  const height = 480;

  // D3 Equirectangular / Natural Earth Projection
  const projection = d3.geoEquirectangular()
    .scale(152)
    .translate([width / 2, height / 2 + 15]);

  const pathGenerator = d3.geoPath().projection(projection);

  // Layer Groups
  const landGroup = svg.append('g').attr('class', 'layer-land');
  const arcsGroup = svg.append('g').attr('class', 'layer-arcs');
  const hubsGroup = svg.append('g').attr('class', 'layer-hubs');
  const userGroup = svg.append('g').attr('class', 'layer-user');

  let currentUserCoords = [77.2090, 28.6139]; // Default coordinates until GPS triggers
  let userPinRendered = false;

  // ── Render Landmass ────────────────────────────────────────────────────────
  function renderWorldMap() {
    // Fetch standard TopoJSON 110m world atlas
    fetch('https://unpkg.com/world-atlas@2.0.2/countries-110m.json')
      .then(res => res.json())
      .then(worldData => {
        const countries = topojson.feature(worldData, worldData.objects.countries).features;
        
        landGroup.selectAll('path')
          .data(countries)
          .enter()
          .append('path')
          .attr('class', 'country-path')
          .attr('d', pathGenerator);

        renderHubsAndArcs();
        initializeGeolocation();
      })
      .catch(err => {
        console.warn('[RADAR] CDN fetch failed, rendering fallback vector grid:', err);
        renderFallbackLand();
        renderHubsAndArcs();
        initializeGeolocation();
      });
  }

  // Fallback vector land contours if offline
  function renderFallbackLand() {
    // Generate approximate continental polygons for offline preview
    const continents = [
      // North America
      [[-165, 65], [-140, 70], [-100, 75], [-60, 60], [-70, 45], [-80, 25], [-95, 20], [-110, 25], [-125, 40], [-160, 55], [-165, 65]],
      // South America
      [[-80, 10], [-50, 0], [-35, -5], [-40, -25], [-55, -50], [-70, -55], [-75, -20], [-80, 10]],
      // Eurasia
      [[-10, 36], [0, 50], [30, 60], [60, 70], [100, 75], [140, 65], [170, 60], [140, 35], [120, 25], [100, 10], [80, 10], [70, 25], [45, 15], [35, 30], [10, 38], [-10, 36]],
      // Africa
      [[-15, 35], [30, 32], [50, 12], [40, -5], [30, -35], [18, -35], [10, 0], [-15, 15], [-15, 35]],
      // Australia
      [[115, -22], [135, -12], [150, -22], [150, -38], [130, -35], [115, -35], [115, -22]]
    ];

    continents.forEach(poly => {
      const geoPoly = { type: 'Polygon', coordinates: [poly] };
      landGroup.append('path')
        .datum(geoPoly)
        .attr('class', 'country-path')
        .attr('d', pathGenerator);
    });
  }

  // ── Render Global Tech Hubs & Glowing Flight Arcs ──────────────────────────
  function renderHubsAndArcs() {
    // Render Arcs between hubs
    HUB_CONNECTIONS.forEach((pair, idx) => {
      const source = GLOBAL_HUBS[pair[0]].coords;
      const target = GLOBAL_HUBS[pair[1]].coords;
      drawCurveArc(source, target, idx % 2 === 0 ? 'data-arc' : 'data-arc secondary');
    });

    // Render Hub Points & Pulses
    GLOBAL_HUBS.forEach(hub => {
      const pos = projection(hub.coords);
      if (!pos) return;

      // Pulsing Ring
      hubsGroup.append('circle')
        .attr('class', 'hub-pulse')
        .attr('cx', pos[0])
        .attr('cy', pos[1])
        .attr('r', 4);

      // Core Dot
      hubsGroup.append('circle')
        .attr('class', 'hub-node')
        .attr('cx', pos[0])
        .attr('cy', pos[1])
        .attr('r', 3)
        .append('title')
        .text(`${hub.name} // LATENCY: ${hub.ping}`);
    });
  }

  // Draws curved Great-Circle flight arcs
  function drawCurveArc(source, target, className) {
    const p1 = projection(source);
    const p2 = projection(target);
    if (!p1 || !p2) return;

    // Bézier control point pulled slightly towards top
    const dx = p2[0] - p1[0];
    const dy = p2[1] - p1[1];
    const dr = Math.sqrt(dx * dx + dy * dy);
    const mx = (p1[0] + p2[0]) / 2;
    const my = (p1[1] + p2[1]) / 2 - dr * 0.22;

    const pathData = `M ${p1[0]} ${p1[1]} Q ${mx} ${my} ${p2[0]} ${p2[1]}`;

    arcsGroup.append('path')
      .attr('class', className)
      .attr('d', pathData);
  }

  // ── Dynamic Geolocation & User Pin ─────────────────────────────────────────
  function initializeGeolocation() {
    if ('geolocation' in navigator) {
      navigator.geolocation.getCurrentPosition(
        position => {
          const lat = position.coords.latitude;
          const lon = position.coords.longitude;
          currentUserCoords = [lon, lat];
          plotUserPosition(lon, lat, 'GPS_ACCURATE');
        },
        error => {
          console.warn('[RADAR] Geolocation access denied or timed out:', error.message);
          // Fallback to IP or Delhi/London default
          plotUserPosition(currentUserCoords[0], currentUserCoords[1], 'ESTIMATED_RELAY');
        },
        { timeout: 8000, enableHighAccuracy: true }
      );
    } else {
      plotUserPosition(currentUserCoords[0], currentUserCoords[1], 'DEFAULT_NODE');
    }
  }

  function plotUserPosition(lon, lat, accuracyType) {
    const pos = projection([lon, lat]);
    if (!pos) return;

    userGroup.selectAll('*').remove();

    // 1. Expanding Concentric Radar Ripple Rings (@keyframes userPing)
    userGroup.append('circle')
      .attr('class', 'user-radar-ring')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 6);

    userGroup.append('circle')
      .attr('class', 'user-radar-ring ring-2')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 6);

    userGroup.append('circle')
      .attr('class', 'user-radar-ring ring-3')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 6);

    // 2. Center Solid Cyan Ping
    userGroup.append('circle')
      .attr('class', 'user-radar-ping')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 4.5);

    // 3. Connect animated data arcs from User location to nearest global hubs
    drawCurveArc([lon, lat], GLOBAL_HUBS[2].coords, 'data-arc'); // to Frankfurt
    drawCurveArc([lon, lat], GLOBAL_HUBS[4].coords, 'data-arc secondary'); // to Singapore

    // 4. Update Tethered Telemetry HUD Callout
    updateTelemetryCallout(pos[0], pos[1], lon, lat, accuracyType);
  }

  function updateTelemetryCallout(x, y, lon, lat, accuracy) {
    const callout = document.getElementById('user-telemetry-callout');
    const coordsEl = document.getElementById('callout-coords');
    const regionEl = document.getElementById('callout-region');
    const bottomCoords = document.getElementById('bottom-coords');

    const latDir = lat >= 0 ? 'N' : 'S';
    const lonDir = lon >= 0 ? 'E' : 'W';
    const formatted = `${Math.abs(lat).toFixed(4)}° ${latDir}, ${Math.abs(lon).toFixed(4)}° ${lonDir}`;

    coordsEl.innerText = `// COORD: ${formatted}`;
    regionEl.innerText = `// ACCURACY: [${accuracy}] // LATENCY: 18ms`;
    if (bottomCoords) bottomCoords.innerText = formatted;

    // Convert SVG coordinates to viewport container percentage / pixels
    const viewport = document.getElementById('radar-viewport');
    const vpRect = viewport.getBoundingClientRect();
    const scaleX = vpRect.width / width;
    const scaleY = vpRect.height / height;

    const screenX = x * scaleX;
    const screenY = y * scaleY;

    // Keep callout inside viewport boundaries
    const safeX = Math.min(Math.max(screenX, 40), vpRect.width - 280);
    const safeY = Math.min(Math.max(screenY, 80), vpRect.height - 120);

    callout.style.left = `${safeX}px`;
    callout.style.top = `${safeY}px`;
    callout.style.opacity = '1';
  }

  // ── Telemetry Live Tickers ────────────────────────────────────────────────
  function startLiveTelemetry() {
    const livePing = document.getElementById('hud-live-ping');
    const freqEl = document.getElementById('radar-freq');

    setInterval(() => {
      const ping = Math.floor(Math.random() * 12) + 16;
      if (livePing) livePing.innerText = `${ping}ms`;
      
      const pingRow = document.getElementById('callout-ping');
      if (pingRow) pingRow.innerText = `// PING: ${ping}ms | STATUS: CONNECTED`;

      // Frequency drift
      const freq = (142.80 + Math.random() * 0.15).toFixed(2);
      if (freqEl) freqEl.innerText = `FREQ: ${freq} MHz`;
    }, 2800);
  }

  // ── Button Actions & License Tester ────────────────────────────────────────
  function setupInteractiveControls() {
    // Re-center / GPS button
    const btnRecenter = document.getElementById('btn-recenter');
    if (btnRecenter) {
      btnRecenter.addEventListener('click', () => {
        btnRecenter.innerHTML = '<span class="radar-scan-icon">⚡</span> SCANNING...';
        initializeGeolocation();
        setTimeout(() => {
          btnRecenter.innerHTML = '<span class="radar-scan-icon">⌖</span> ACQUIRE_GPS';
        }, 1500);
      });
    }

    // License key verifier simulator
    const btnTest = document.getElementById('btn-test-license');
    const inputKey = document.getElementById('license-input');
    const resultBox = document.getElementById('tester-result');

    if (btnTest && inputKey && resultBox) {
      btnTest.addEventListener('click', () => {
        const key = inputKey.value.trim().toUpperCase();
        if (!key) {
          resultBox.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Please enter a license key string.';
          return;
        }

        resultBox.innerHTML = '<span style="color:#ffa502;">VERIFYING:</span> Interrogating encrypted license authority...';

        setTimeout(() => {
          if (key.includes('PRO') || key.includes('SHADOW') || key.length > 8) {
            resultBox.innerHTML = `
              <span style="color:#00ff66; font-weight:bold;">[SUCCESS: ACTIVE]</span>
              Key validated: <code>${key}</code> • Tier: <strong>SHADOW PRO [UNLIMITED]</strong> • Model: GPT-4o / Claude 3.5 Sonnet Nodes Enabled.
            `;
          } else {
            resultBox.innerHTML = `
              <span style="color:#ff4757;">[INVALID KEY]</span>
              Key could not be resolved. Use option A (Free Gemini BYOK) or purchase an instant automated Pro key.
            `;
          }
        }, 900);
      });

      inputKey.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
          btnTest.click();
        }
      });
    }

    // ── UPI Checkout Modal Logic ─────────────────────────────────────────────
    const upiModal = document.getElementById('upi-modal');
    const buyBtn = document.getElementById('btn-buy-license');
    const upiModalClose = document.getElementById('upi-modal-close');
    const btnCopyUpi = document.getElementById('btn-copy-upi');
    const upiVpaText = document.getElementById('upi-vpa');
    const btnClaimKey = document.getElementById('btn-claim-license');
    const claimEmailInput = document.getElementById('claim-email-input');
    const claimUtrInput = document.getElementById('claim-utr-input');
    const btnWhatsapp = document.getElementById('btn-whatsapp-activate');
    const claimOutput = document.getElementById('claim-output');

    function updateWhatsAppUrl() {
      if (!btnWhatsapp) return;
      const em = claimEmailInput ? claimEmailInput.value.trim() : '';
      const utr = claimUtrInput ? claimUtrInput.value.trim() : '';
      const msg = `Hi Harshil, I have paid ₹99 for ShadowAI Pro!%0A%0AMy Google Email: ${encodeURIComponent(em || '[Enter your email]')}%0AMy 12-Digit UPI UTR: ${encodeURIComponent(utr || '[Attaching payment screenshot]')}`;
      btnWhatsapp.href = `https://wa.me/919317526356?text=${msg}`;
    }

    if (claimEmailInput) claimEmailInput.addEventListener('input', updateWhatsAppUrl);
    if (claimUtrInput) claimUtrInput.addEventListener('input', updateWhatsAppUrl);

    if (buyBtn && upiModal) {
      buyBtn.addEventListener('click', (e) => {
        e.preventDefault();
        upiModal.classList.add('open');
        updateWhatsAppUrl();
      });
    }

    if (upiModalClose && upiModal) {
      upiModalClose.addEventListener('click', () => {
        upiModal.classList.remove('open');
      });
    }

    // Close on backdrop click
    if (upiModal) {
      upiModal.addEventListener('click', (e) => {
        if (e.target === upiModal) upiModal.classList.remove('open');
      });
    }

    // Copy UPI ID to clipboard
    if (btnCopyUpi && upiVpaText) {
      btnCopyUpi.addEventListener('click', () => {
        navigator.clipboard.writeText(upiVpaText.innerText.trim()).then(() => {
          const orig = btnCopyUpi.innerText;
          btnCopyUpi.innerText = 'COPIED ✓';
          btnCopyUpi.style.color = '#00ff66';
          btnCopyUpi.style.borderColor = '#00ff66';
          setTimeout(() => {
            btnCopyUpi.innerText = orig;
            btnCopyUpi.style.color = '';
            btnCopyUpi.style.borderColor = '';
          }, 2000);
        });
      });
    }

    // Submit UTR and Log for Verification
    if (btnClaimKey && claimEmailInput && claimOutput) {
      btnClaimKey.addEventListener('click', () => {
        const email = claimEmailInput.value.trim();
        const utr = claimUtrInput ? claimUtrInput.value.trim() : '';
        if (!email || !email.includes('@')) {
          claimOutput.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Please enter your valid email address.';
          return;
        }
        if (!utr || utr.length < 8) {
          claimOutput.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Please enter your 12-digit UPI UTR / Ref number from your payment receipt.';
          return;
        }

        btnClaimKey.disabled = true;
        btnClaimKey.innerText = 'LOGGING...';
        claimOutput.innerHTML = '<span style="color:#00e5ff;">RECORDING:</span> Registering transaction in cloud verification database...';

        localStorage.setItem('shadow_user_email', email);
        localStorage.setItem('shadow_user_utr', utr);

        // Sync to Supabase Cloud Database
        try {
          fetch(`${SUPABASE_URL}/rest/v1/licenses`, {
            method: 'POST',
            headers: {
              'apikey': SUPABASE_KEY,
              'Authorization': `Bearer ${SUPABASE_KEY}`,
              'Content-Type': 'application/json',
              'Prefer': 'return=minimal'
            },
            body: JSON.stringify({
              license_key: `PENDING-${utr}`,
              customer_email: email,
              plan_tier: 'PRO_MONTHLY',
              is_active: false
            })
          }).catch(e => console.warn('Supabase sync notice:', e));
        } catch (e) {}

        setTimeout(() => {
          updateWhatsAppUrl();
          claimOutput.innerHTML = `
            <div style="color:#00ff66;font-weight:700;margin-bottom:6px;">✓ PAYMENT SUBMITTED FOR APPROVAL</div>
            <div style="font-size:13px;letter-spacing:1px;color:#00e5ff;padding:8px;background:#030805;border:1px dashed #00ff66;border-radius:4px;font-family:monospace;margin:6px 0;text-align:center;">
              UTR: <strong>${utr}</strong>
            </div>
            <p style="color:#e2fced;font-size:11px;margin:6px 0;">Assigned to: <strong>${email}</strong></p>
            <p style="color:#7ca88e;font-size:11px;margin-bottom:8px;">Now click the green <strong>SEND PROOF ON WHATSAPP</strong> button above to get your key activated by Harshil in ~1 minute!</p>
            <a href="${btnWhatsapp ? btnWhatsapp.href : 'https://wa.me/919317526356'}" target="_blank" style="display:block;background:#25D366;color:#ffffff;text-align:center;font-weight:bold;padding:9px;border-radius:4px;text-decoration:none;font-family:monospace;">
              OPEN WHATSAPP NOW 💬
            </a>
          `;
          btnClaimKey.disabled = false;
          btnClaimKey.innerText = 'SUBMIT_UTR ⯈';
        }, 800);
      });
    }


    // Google SSO via Supabase Client
    const supabaseClient = (window.supabase && typeof window.supabase.createClient === 'function')
      ? window.supabase.createClient(SUPABASE_URL, SUPABASE_KEY)
      : null;

    const authModal = document.getElementById('auth-modal');
    const btnGoogleAuth = document.getElementById('btn-google-auth');
    const btnAuthLabel = document.getElementById('btn-auth-label');
    const modalClose = document.getElementById('modal-close');
    const googleAction = document.getElementById('google-sso-action');
    const authStatusLog = document.getElementById('auth-status-log');
    const navAdminLink = document.getElementById('nav-admin-link');

    async function checkWebSession() {
      if (!supabaseClient) return;
      try {
        const { data: { session } } = await supabaseClient.auth.getSession();
        if (session && session.user && session.user.email) {
          const email = session.user.email;
          if (btnAuthLabel) btnAuthLabel.innerText = email.split('@')[0].toUpperCase();
          if (navAdminLink && (email.includes('harshil') || email === 'harshilthakur82@gmail.com')) {
            navAdminLink.style.display = 'inline-flex';
          }
        }
      } catch(e) {
        console.warn('Session check:', e);
      }
    }
    checkWebSession();

    if (btnGoogleAuth && authModal) {
      btnGoogleAuth.addEventListener('click', () => {
        authModal.classList.add('open');
      });
    }

    if (modalClose && authModal) {
      modalClose.addEventListener('click', () => {
        authModal.classList.remove('open');
      });
    }

    if (googleAction) {
      googleAction.addEventListener('click', async () => {
        if (authStatusLog) authStatusLog.innerHTML = '<span style="color:#00e5ff;">REDIRECTING:</span> Connecting to Google Secure OAuth...';
        if (supabaseClient) {
          try {
            await supabaseClient.auth.signInWithOAuth({
              provider: 'google',
              options: {
                redirectTo: window.location.origin
              }
            });
          } catch(err) {
            if (authStatusLog) authStatusLog.innerHTML = '<span style="color:#ff4757;">ERROR:</span> ' + err.message;
          }
        }
      });
    }

    // Download mock alert
    const dlWin = document.getElementById('btn-dl-windows');
    const dlMac = document.getElementById('btn-dl-mac');

    if (dlWin) {
      dlWin.addEventListener('click', (e) => {
        e.preventDefault();
        window.location.href = 'downloads/ShadowAI_Setup.exe';
      });
    }

    if (dlMac) {
      dlMac.addEventListener('click', (e) => {
        e.preventDefault();
        window.location.href = 'downloads/ShadowAI.dmg';
      });
    }
  }

  // ── Initialize on DOM ready ───────────────────────────────────────────────
  window.addEventListener('DOMContentLoaded', () => {
    renderWorldMap();
    startLiveTelemetry();
    setupInteractiveControls();
  });

})();
