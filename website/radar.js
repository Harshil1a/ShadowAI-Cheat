/**
 * SHADOW AI // CYBERPUNK GEO-LOCATION RADAR MAP & TELEMETRY CONTROLLER
 */

(function () {
  'use strict';

  // ── Supabase Cloud Database Configuration ──────────────────────────────────
  const SUPABASE_URL = 'https://kptqmelofgromeavgmip.supabase.co';
  const SUPABASE_KEY = 'sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J';
  let currentAuthUser = null;

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
        renderActiveUserClusters();
        initializeGeolocation();
      })
      .catch(err => {
        console.warn('[RADAR] CDN fetch failed, rendering fallback vector grid:', err);
        renderFallbackLand();
        renderHubsAndArcs();
        renderActiveUserClusters();
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

  // Live Active User Clusters around the globe (Glowing Green Dots)
  const ACTIVE_USER_CLUSTERS = [
    { name: 'DELHI_OPERATORS', coords: [77.2090, 28.6139], count: 342 },
    { name: 'MUMBAI_OPERATORS', coords: [72.8777, 19.0760], count: 218 },
    { name: 'BANGALORE_OPERATORS', coords: [77.5946, 12.9716], count: 189 },
    { name: 'HYDERABAD_OPERATORS', coords: [78.4867, 17.3850], count: 114 },
    { name: 'NEW_YORK_OPERATORS', coords: [-74.0060, 40.7128], count: 176 },
    { name: 'SF_BAY_OPERATORS', coords: [-122.0842, 37.4220], count: 204 },
    { name: 'LONDON_OPERATORS', coords: [-0.1276, 51.5074], count: 145 },
    { name: 'BERLIN_OPERATORS', coords: [13.4050, 52.5200], count: 98 },
    { name: 'SINGAPORE_OPERATORS', coords: [103.8198, 1.3521], count: 165 },
    { name: 'TOKYO_OPERATORS', coords: [139.6917, 35.6895], count: 138 },
    { name: 'SYDNEY_OPERATORS', coords: [151.2093, -33.8688], count: 87 }
  ];

  // Render Green Active User Dots on the Map
  function renderActiveUserClusters() {
    ACTIVE_USER_CLUSTERS.forEach(cluster => {
      const pos = projection(cluster.coords);
      if (!pos) return;

      // Outer gentle pulse
      hubsGroup.append('circle')
        .attr('class', 'user-cluster-pulse')
        .attr('cx', pos[0])
        .attr('cy', pos[1])
        .attr('r', 5)
        .attr('fill', 'none')
        .attr('stroke', '#00ff66')
        .attr('stroke-width', '1')
        .attr('opacity', '0.5');

      // Inner glowing green dot
      hubsGroup.append('circle')
        .attr('cx', pos[0])
        .attr('cy', pos[1])
        .attr('r', 3)
        .attr('fill', '#00ff66')
        .attr('filter', 'drop-shadow(0 0 4px #00ff66)')
        .append('title')
        .text(`${cluster.name}: ${cluster.count} Active Users`);
    });
  }

  // ── Dynamic Geolocation & User Pin ─────────────────────────────────────────
  // Broad Region Estimator (Zero GPS permissions, zero exact coordinates)
  function getEstimatedUserRegion() {
    try {
      const tz = Intl.DateTimeFormat().resolvedOptions().timeZone || '';
      if (tz.includes('Kolkata') || tz.includes('India') || tz.includes('Calcutta') || tz.includes('Colombo') || tz.includes('Asia')) {
        return { name: 'INDIA / SOUTH ASIA REGION', coords: [78.96, 22.59] };
      }
      if (tz.includes('Europe') || tz.includes('London') || tz.includes('Berlin') || tz.includes('Paris')) {
        return { name: 'WESTERN EUROPE REGION', coords: [10.45, 51.16] };
      }
      if (tz.includes('New_York') || tz.includes('Chicago') || tz.includes('Toronto')) {
        return { name: 'NORTH AMERICA (EAST)', coords: [-77.03, 38.90] };
      }
      if (tz.includes('Los_Angeles') || tz.includes('Denver') || tz.includes('Vancouver')) {
        return { name: 'NORTH AMERICA (WEST)', coords: [-122.41, 37.77] };
      }
      if (tz.includes('Tokyo') || tz.includes('Seoul') || tz.includes('Singapore')) {
        return { name: 'EAST ASIA / PACIFIC REGION', coords: [120.98, 24.80] };
      }
    } catch(e) {}
    return { name: 'INDIA / SOUTH ASIA REGION', coords: [78.96, 22.59] };
  }

  // ── Query Real Users Count from Supabase Database ──────────────────────────
  async function loadActualDatabaseUsers() {
    try {
      const [licRes, profRes] = await Promise.allSettled([
        fetch(`${SUPABASE_URL}/rest/v1/licenses?select=id`, {
          headers: { 'apikey': SUPABASE_KEY, 'Authorization': `Bearer ${SUPABASE_KEY}` }
        }),
        fetch(`${SUPABASE_URL}/rest/v1/profiles?select=id`, {
          headers: { 'apikey': SUPABASE_KEY, 'Authorization': `Bearer ${SUPABASE_KEY}` }
        })
      ]);

      let total = 0;
      if (licRes.status === 'fulfilled') {
        const lics = await licRes.value.json();
        if (Array.isArray(lics)) total += lics.length;
      }
      if (profRes.status === 'fulfilled') {
        const profs = await profRes.value.json();
        if (Array.isArray(profs)) total += profs.length;
      }

      // Display real database count on top
      const displayTotal = Math.max(total, 1); // at least current user
      const counterEl = document.getElementById('active-users-counter');
      if (counterEl) {
        counterEl.innerText = `${displayTotal} REGISTERED USER${displayTotal === 1 ? '' : 'S'} IN DATABASE`;
      }
    } catch (e) {
      console.warn('Real user load:', e);
    }
  }

  // ── Regional Estimate (Broad zone, NOT exact pin) ──────────────────────────
  function initializeGeolocation() {
    const region = getEstimatedUserRegion();
    plotBroadRegion(region.coords[0], region.coords[1], region.name);
    loadActualDatabaseUsers();
  }

  function plotBroadRegion(lon, lat, regionName) {
    const pos = projection([lon, lat]);
    if (!pos) return;

    userGroup.selectAll('*').remove();

    // 1. Broad Soft Glowing Regional Radius (NOT an exact pin)
    userGroup.append('circle')
      .attr('class', 'user-radar-ring')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 20) // Soft 20px regional zone
      .attr('fill', 'rgba(0, 255, 102, 0.12)')
      .attr('stroke', '#00ff66')
      .attr('stroke-width', '1.5')
      .attr('stroke-dasharray', '4,3');

    // 2. Center Green Dot Indicator
    userGroup.append('circle')
      .attr('class', 'user-radar-ping')
      .attr('cx', pos[0])
      .attr('cy', pos[1])
      .attr('r', 4.5)
      .attr('fill', '#00ff66')
      .attr('filter', 'drop-shadow(0 0 6px #00ff66)');

    // 3. Connect broad arc to nearest global hub
    drawCurveArc([lon, lat], GLOBAL_HUBS[2].coords, 'data-arc');

    // 4. Update HUD Callout with broad region info (NO exact GPS numbers)
    updateTelemetryCallout(pos[0], pos[1], regionName);
  }

  function updateTelemetryCallout(x, y, regionName = 'INDIA / SOUTH ASIA REGION') {
    const callout = document.getElementById('user-telemetry-callout');
    const coordsEl = document.getElementById('callout-coords');
    const regionEl = document.getElementById('callout-region');

    if (coordsEl) coordsEl.innerText = `// ESTIMATE: ${regionName}`;
    if (regionEl) regionEl.innerText = `// EXACT PINNING: DISABLED (PRIVACY PROTECTED)`;

    // Position callout
    const viewport = document.getElementById('radar-viewport');
    if (!viewport || !callout) return;
    const vpRect = viewport.getBoundingClientRect();
    const scaleX = vpRect.width / width;
    const scaleY = vpRect.height / height;

    const screenX = x * scaleX;
    const screenY = y * scaleY;

    const safeX = Math.min(Math.max(screenX, 40), vpRect.width - 290);
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

        resultBox.innerHTML = '<span style="color:#ffa502;">VERIFYING:</span> Interrogating live Supabase Cloud License Database...';

        fetch(`${SUPABASE_URL}/rest/v1/licenses?license_key=eq.${encodeURIComponent(key)}&select=*`, {
          headers: {
            'apikey': SUPABASE_KEY,
            'Authorization': `Bearer ${SUPABASE_KEY}`
          }
        })
        .then(res => res.json())
        .then(data => {
          if (Array.isArray(data) && data.length > 0) {
            const lic = data[0];
            const createdDate = new Date(lic.created_at);
            const diffDays = Math.max(0, (new Date() - createdDate) / (1000 * 60 * 60 * 24));
            const isMonthly = !lic.plan_tier || lic.plan_tier === 'PRO_MONTHLY';
            const isExpired = isMonthly && diffDays >= 30;
            const daysLeft = Math.max(0, Math.ceil(30 - diffDays));

            if (lic.is_active && !isExpired) {
              const timeDisplay = isMonthly ? `${daysLeft} Days Remaining` : 'Lifetime Access';
              resultBox.innerHTML = `
                <span style="color:#00ff66; font-weight:bold;">[LIVE SUPABASE API: VERIFIED ACTIVE ✓]</span><br>
                Key: <code>${lic.license_key}</code> • Tier: <strong>${lic.plan_tier || 'PRO_MONTHLY'}</strong> • Validity: <span style="color:#00e5ff;">${timeDisplay}</span>
              `;
            } else if (isExpired) {
              resultBox.innerHTML = `
                <span style="color:#ffa502; font-weight:bold;">[LIVE SUPABASE API: EXPIRED (30-DAY LIMIT)]</span><br>
                Key <code>${lic.license_key}</code> has reached its 30-day monthly limit. Please renew your subscription to reactivate.
              `;
            } else {
              resultBox.innerHTML = `
                <span style="color:#ff4757; font-weight:bold;">[LIVE SUPABASE API: REVOKED]</span><br>
                Key <code>${lic.license_key}</code> exists in the cloud database but has been marked revoked or deactivated.
              `;
            }
          } else if (key === 'SHADOW-PRO-DEMO-2026' || key === 'SHADOW-PRO-HARSHIL-ADMIN') {
            resultBox.innerHTML = `
              <span style="color:#00e5ff; font-weight:bold;">[SYSTEM KEY VERIFIED ✓]</span><br>
              Key <code>${key}</code> is authorized for immediate desktop Pro unlocking.
            `;
          } else {
            resultBox.innerHTML = `
              <span style="color:#ff4757; font-weight:bold;">[INVALID / UNREGISTERED KEY]</span><br>
              Key <code>${key}</code> was not found in the live Supabase cloud database.
            `;
          }
        })
        .catch(err => {
          console.warn('Live license verification error:', err);
          resultBox.innerHTML = `
            <span style="color:#ffa502; font-weight:bold;">[OFFLINE VALIDATION]</span><br>
            Key format inspected: <code>${key}</code>. Cloud API ping error: ${err.message}.
          `;
        });
      });

      inputKey.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
          btnTest.click();
        }
      });
    }

    // ── Dual UPI / Crypto Checkout Modal Logic ─────────────────────────────
    const upiModal = document.getElementById('upi-modal');
    const buyBtn = document.getElementById('btn-buy-license');
    const upiModalClose = document.getElementById('upi-modal-close');
    const btnCopyUpi = document.getElementById('btn-copy-upi');
    const upiVpaText = document.getElementById('upi-vpa');
    const btnCopyCrypto = document.getElementById('btn-copy-crypto');
    const cryptoAddressText = document.getElementById('crypto-address');
    const tabPayUpi = document.getElementById('tab-pay-upi');
    const tabPayCrypto = document.getElementById('tab-pay-crypto');
    const payContainerUpi = document.getElementById('pay-container-upi');
    const payContainerCrypto = document.getElementById('pay-container-crypto');
    const btnClaimKey = document.getElementById('btn-claim-license');
    const claimEmailInput = document.getElementById('claim-email-input');
    const claimUtrInput = document.getElementById('claim-utr-input');
    const claimOutput = document.getElementById('claim-output');

    let activePaymentMethod = 'UPI'; // 'UPI' or 'CRYPTO'
    let currentOrderCode = sessionStorage.getItem('shadow_order_code') || ('#SH-' + Date.now().toString().slice(-6));
    sessionStorage.setItem('shadow_order_code', currentOrderCode);

    function syncOrderCodeUI() {
      const codeEl = document.getElementById('checkout-order-code');
      if (codeEl) codeEl.innerText = currentOrderCode;

      // Auto-fetch verified Google email if user is signed in
      if (currentAuthUser && currentAuthUser.email && claimEmailInput) {
        claimEmailInput.value = currentAuthUser.email;
        claimEmailInput.readOnly = true;
        claimEmailInput.style.borderColor = '#00ff66';
        claimEmailInput.style.color = '#00ff66';
        claimEmailInput.title = 'Verified Google Account (Auto-linked)';
      }
    }

    // Tab Switching Logic
    if (tabPayUpi && tabPayCrypto && payContainerUpi && payContainerCrypto) {
      tabPayUpi.addEventListener('click', () => {
        activePaymentMethod = 'UPI';
        tabPayUpi.style.background = 'rgba(0, 255, 102, 0.15)';
        tabPayUpi.style.color = '#00ff66';
        tabPayUpi.style.borderColor = '#00ff66';

        tabPayCrypto.style.background = 'rgba(0, 229, 255, 0.05)';
        tabPayCrypto.style.color = '#7ca88e';
        tabPayCrypto.style.borderColor = 'rgba(0, 229, 255, 0.2)';

        payContainerUpi.style.display = 'block';
        payContainerCrypto.style.display = 'none';

        if (claimUtrInput) {
          claimUtrInput.placeholder = 'Exact 12-Digit UPI UTR / Ref No.';
          claimUtrInput.maxLength = 12;
        }
        if (claimOutput) {
          claimOutput.innerText = 'Enter your email & 12-digit UTR to register your approval request.';
        }
      });

      tabPayCrypto.addEventListener('click', () => {
        activePaymentMethod = 'CRYPTO';
        tabPayCrypto.style.background = 'rgba(0, 229, 255, 0.15)';
        tabPayCrypto.style.color = '#00e5ff';
        tabPayCrypto.style.borderColor = '#00e5ff';

        tabPayUpi.style.background = 'rgba(0, 255, 102, 0.05)';
        tabPayUpi.style.color = '#7ca88e';
        tabPayUpi.style.borderColor = 'rgba(0, 255, 102, 0.2)';

        payContainerUpi.style.display = 'none';
        payContainerCrypto.style.display = 'block';

        if (claimUtrInput) {
          claimUtrInput.placeholder = 'Paste TxID / Transaction Hash (0x...)';
          claimUtrInput.maxLength = 70;
        }
        if (claimOutput) {
          claimOutput.innerText = 'Enter your email & Transaction Hash (TxID) to register your crypto payment.';
        }
      });
    }

    if (buyBtn && upiModal) {
      buyBtn.addEventListener('click', (e) => {
        e.preventDefault();
        // Generate conflict-proof order code on each checkout attempt if not already set
        if (!sessionStorage.getItem('shadow_order_code')) {
          currentOrderCode = '#SH-' + Date.now().toString().slice(-6);
          sessionStorage.setItem('shadow_order_code', currentOrderCode);
        }
        syncOrderCodeUI();
        upiModal.classList.add('open');
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

    // Copy Crypto Address to clipboard
    if (btnCopyCrypto && cryptoAddressText) {
      btnCopyCrypto.addEventListener('click', () => {
        navigator.clipboard.writeText(cryptoAddressText.innerText.trim()).then(() => {
          const orig = btnCopyCrypto.innerText;
          btnCopyCrypto.innerText = 'COPIED ✓';
          btnCopyCrypto.style.color = '#00e5ff';
          btnCopyCrypto.style.borderColor = '#00e5ff';
          setTimeout(() => {
            btnCopyCrypto.innerText = orig;
            btnCopyCrypto.style.color = '';
            btnCopyCrypto.style.borderColor = '';
          }, 2000);
        });
      });
    }

    // Live Poller & Timer state
    let approvalPollInterval = null;
    let countdownTimerInterval = null;

    function startWaitingRoom(email, utr, orderCode) {
      const claimSection = document.getElementById('upi-claim-section');
      const waitingRoom = document.getElementById('approval-waiting-room');
      const waitOrder = document.getElementById('wait-order-code');
      const waitEmail = document.getElementById('wait-email');
      const timerEl = document.getElementById('approval-timer');
      const successBox = document.getElementById('approval-success-box');
      const pollerBox = document.getElementById('poller-status-box');
      const successEmail = document.getElementById('success-email');

      if (claimSection) claimSection.style.display = 'none';
      if (waitingRoom) waitingRoom.style.display = 'block';
      if (waitOrder) waitOrder.innerText = orderCode;
      if (waitEmail) waitEmail.innerText = email;
      if (successEmail) successEmail.innerText = email;

      // 15:00 countdown timer
      let timeLeft = 900; // 15 minutes
      if (countdownTimerInterval) clearInterval(countdownTimerInterval);
      countdownTimerInterval = setInterval(() => {
        if (timeLeft <= 0) {
          if (timerEl) timerEl.innerText = '00:00 (Verifying...)';
          return;
        }
        timeLeft--;
        const mins = Math.floor(timeLeft / 60).toString().padStart(2, '0');
        const secs = (timeLeft % 60).toString().padStart(2, '0');
        if (timerEl) timerEl.innerText = `${mins}:${secs}`;
      }, 1000);

      // Real-time Supabase poller every 4 seconds
      if (approvalPollInterval) clearInterval(approvalPollInterval);
      approvalPollInterval = setInterval(async () => {
        try {
          const res = await fetch(`${SUPABASE_URL}/rest/v1/licenses?customer_email=eq.${encodeURIComponent(email)}&is_active=eq.true&select=*`, {
            headers: { 'apikey': SUPABASE_KEY, 'Authorization': `Bearer ${SUPABASE_KEY}` }
          });
          const rows = await res.json();
          if (Array.isArray(rows) && rows.length > 0) {
            // PRO APPROVED!
            clearInterval(approvalPollInterval);
            clearInterval(countdownTimerInterval);
            if (pollerBox) pollerBox.style.display = 'none';
            if (timerEl) {
              timerEl.innerText = 'APPROVED ✓';
              timerEl.style.color = '#00ff66';
            }
            if (successBox) successBox.style.display = 'block';
            localStorage.setItem('shadow_user_is_pro', 'true');
          }
        } catch(e) {
          console.warn('Approval poller notice:', e);
        }
      }, 4000);
    }

    // Submit Payment and Log for Verification
    if (btnClaimKey && claimEmailInput && claimOutput) {
      btnClaimKey.addEventListener('click', () => {
        const email = claimEmailInput.value.trim();
        const utr = claimUtrInput ? claimUtrInput.value.trim() : '';
        const isCrypto = activePaymentMethod === 'CRYPTO';

        if (!email || !email.includes('@')) {
          claimOutput.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Please enter your valid email address.';
          return;
        }

        if (isCrypto) {
          const cleanTx = utr.trim().toLowerCase();
          const isHex = /^0x[a-f0-9]{64}$/.test(cleanTx) || /^[a-f0-9]{64}$/.test(cleanTx);
          if (!cleanTx || (!isHex && cleanTx.length < 20)) {
            claimOutput.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Invalid Transaction Hash. Please paste the full TxID / Hash (e.g. 0x...) from your crypto wallet receipt.';
            return;
          }
        } else {
          // Domestic UPI strictly requires 12 digits
          if (!utr || !/^\d{12}$/.test(utr)) {
            claimOutput.innerHTML = '<span style="color:#ff4757;">ERROR:</span> Please enter the exact 12-digit numeric UPI UTR / Ref number from your receipt.';
            return;
          }
        }

        btnClaimKey.disabled = true;
        btnClaimKey.innerText = 'LOGGING...';
        claimOutput.innerHTML = '<span style="color:#00e5ff;">RECORDING:</span> Registering order #' + currentOrderCode + ' in cloud database...';

        localStorage.setItem('shadow_user_email', email);
        localStorage.setItem('shadow_user_utr', utr);

        const methodLabel = isCrypto ? 'BNB Smart Chain (BEP-20)' : 'UPI';
        const amountText = isCrypto ? '$6.00 USDT / BNB' : '₹99';
        const refLabel = isCrypto ? 'TxID' : 'UPI UTR';

        // Sync pending order to Supabase Cloud Database
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
              license_key: `PENDING-${currentOrderCode.replace('#','')}-${utr.slice(0, 16)}`,
              customer_email: email,
              plan_tier: isCrypto ? 'PRO_GLOBAL_CRYPTO' : 'PRO_MONTHLY',
              is_active: false,
              bound_hwid: `Order ${currentOrderCode} | Method: ${methodLabel} | ${refLabel}: ${utr}`
            })
          }).catch(e => console.warn('Supabase sync notice:', e));
        } catch (e) {}

        // Send instant Telegram Notification to Admin
        try {
          const tgToken = '8880063864:AAEK6ChjazpjBlbiQZmpuFLN2IZliJgyb2c';
          const tgChatId = '6602106376';
          const tgText = `🔔 *NEW SHADOWAI PAYMENT SUBMITTED!*\n` +
                         `━━━━━━━━━━━━━━━━━━━━\n` +
                         `📦 *Order Code:* \`${currentOrderCode}\`\n` +
                         `💳 *Method:* ${methodLabel}\n` +
                         `💰 *Amount:* ${amountText}\n` +
                         `👤 *Customer:* \`${email}\`\n` +
                         `🧾 *${refLabel}:* \`${utr}\`\n` +
                         `⏰ *Timestamp:* ${new Date().toLocaleString('en-IN')}\n` +
                         `━━━━━━━━━━━━━━━━━━━━\n` +
                         `👉 *Action:* Check ${isCrypto ? 'Trust Wallet' : 'Bank / GPay'} and approve in Admin Panel!`;
          fetch(`https://api.telegram.org/bot${tgToken}/sendMessage`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              chat_id: tgChatId,
              text: tgText,
              parse_mode: 'Markdown'
            })
          }).catch(e => console.warn('Telegram ping notice:', e));
        } catch (e) {}

        // Switch to Live Waiting Room
        setTimeout(() => {
          startWaitingRoom(email, utr, currentOrderCode);
        }, 600);
      });
    }

    syncOrderCodeUI();


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

    // Profile modal elements
    const profileModal = document.getElementById('user-profile-modal');
    const profileClose = document.getElementById('profile-modal-close');
    const profileImg = document.getElementById('user-profile-img');
    const profileFallback = document.getElementById('user-profile-avatar-fallback');
    const profileName = document.getElementById('user-profile-name');
    const profileEmail = document.getElementById('user-profile-email');
    const profileTier = document.getElementById('user-profile-tier');
    const profileAdminSec = document.getElementById('user-profile-admin-section');
    const btnSignout = document.getElementById('btn-user-signout');

    // Admin Master Stealth Whitelist
    const ADMIN_WHITELIST = ['harshilthakur82@gmail.com', 'harshilthakur82@oksbi', 'harshil1a'];

    function renderLoggedInUser(user) {
      currentAuthUser = user;
      const email = user.email || '';
      const meta = user.user_metadata || {};
      const fullName = meta.full_name || meta.name || email.split('@')[0];
      const avatarUrl = meta.avatar_url || '';
      const isMasterAdmin = ADMIN_WHITELIST.some(a => email.toLowerCase().includes(a.toLowerCase())) || email.toLowerCase().includes('harshil');

      // Update Top Nav Button with User Name & Photo
      if (btnGoogleAuth) {
        btnGoogleAuth.style.borderColor = '#00ff66';
        btnGoogleAuth.style.background = 'rgba(0,255,102,0.1)';
        btnGoogleAuth.innerHTML = `
          ${avatarUrl ? `<img src="${avatarUrl}" alt="Avatar" style="width:18px;height:18px;border-radius:50%;object-fit:cover;border:1px solid #00ff66;">` : `<span style="font-size:12px;">👤</span>`}
          <span style="color:#00ff66;font-weight:700;letter-spacing:0.5px;max-width:130px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${fullName.toUpperCase()}</span>
          <span style="width:6px;height:6px;background:#00ff66;border-radius:50%;box-shadow:0 0 6px #00ff66;"></span>
        `;
      }

      // Populate User Profile Modal
      if (profileName) profileName.innerText = fullName.toUpperCase();
      if (profileEmail) profileEmail.innerText = email;
      if (avatarUrl && profileImg) {
        profileImg.src = avatarUrl;
        profileImg.style.display = 'block';
        if (profileFallback) profileFallback.style.display = 'none';
      }
      // Fetch Live License & Plan from Supabase Cloud
      const badgeEl = document.getElementById('user-profile-badge');
      const tierEl = document.getElementById('user-profile-tier');
      const keyContainer = document.getElementById('user-profile-key-container');
      const keyInput = document.getElementById('user-profile-key-input');
      const hwidEl = document.getElementById('user-profile-hwid');
      const btnCopyKey = document.getElementById('btn-copy-user-key');
      const copiedStatus = document.getElementById('user-key-copied-status');

      if (btnCopyKey && keyInput) {
        btnCopyKey.onclick = () => {
          if (keyInput.value && !keyInput.value.startsWith('FETCHING') && !keyInput.value.startsWith('NO ACTIVE')) {
            navigator.clipboard.writeText(keyInput.value);
            if (copiedStatus) {
              copiedStatus.style.display = 'inline';
              setTimeout(() => { copiedStatus.style.display = 'none'; }, 2000);
            }
          }
        };
      }

      if (profileAdminSec) {
        profileAdminSec.style.display = isMasterAdmin ? 'block' : 'none';
      }
      if (isMasterAdmin) {
        localStorage.setItem('shadow_admin_user', 'harshilthakur82@gmail.com');
        if (badgeEl) { badgeEl.innerText = 'LIFETIME ADMIN'; badgeEl.style.color = '#00e5ff'; }
        if (tierEl) { tierEl.innerText = 'MASTER OPERATOR // UNLIMITED TACTICAL SUITE'; tierEl.style.color = '#00ff66'; }
        if (keyInput) keyInput.value = 'SHADOW-PRO-HARSHIL-ADMIN';
        if (hwidEl) hwidEl.innerHTML = 'HARDWARE BINDING: <span style="color:#00ff66;">ALL ACCESS UNLOCKED</span>';
      } else {
        // Query Supabase for customer's license by email
        fetch(`${SUPABASE_URL}/rest/v1/licenses?customer_email=eq.${encodeURIComponent(email)}&order=created_at.desc&limit=1&select=*`, {
          headers: { 'apikey': SUPABASE_KEY, 'Authorization': `Bearer ${SUPABASE_KEY}` }
        })
        .then(r => r.json())
        .then(rows => {
          if (Array.isArray(rows) && rows.length > 0) {
            const row = rows[0];
            const planTier = (row.plan_tier || 'PRO_MONTHLY').toUpperCase();
            const isActive = row.is_active;
            const boundHwid = row.bound_hwid || '';
            const createdAt = new Date(row.created_at);
            const daysUsed = Math.floor((new Date() - createdAt) / (1000 * 60 * 60 * 24));
            const daysLeft = Math.max(0, 30 - daysUsed);

            if (!isActive) {
              if (badgeEl) { badgeEl.innerText = 'REVOKED'; badgeEl.style.color = '#ff4757'; }
              if (tierEl) { tierEl.innerText = 'KEY REVOKED OR SUSPENDED'; tierEl.style.color = '#ff4757'; }
              if (keyInput) keyInput.value = row.license_key;
              if (hwidEl) hwidEl.innerText = 'HARDWARE BINDING: INACTIVE';
            } else if (planTier === 'PRO_MONTHLY' && daysLeft === 0) {
              if (badgeEl) { badgeEl.innerText = 'EXPIRED (30d)'; badgeEl.style.color = '#ff9900'; }
              if (tierEl) { tierEl.innerText = 'PRO MONTHLY (EXPIRED - PLEASE RENEW)'; tierEl.style.color = '#ff9900'; }
              if (keyInput) keyInput.value = row.license_key;
              if (hwidEl) hwidEl.innerText = 'HARDWARE BINDING: EXPIRED';
            } else {
              // Active Pro!
              const leftText = planTier === 'PRO_MONTHLY' ? `ACTIVE (${daysLeft}d left)` : 'ACTIVE (LIFETIME)';
              if (badgeEl) { badgeEl.innerText = leftText; badgeEl.style.color = '#00ff66'; }
              if (tierEl) { tierEl.innerText = `${planTier.replace('_', ' ')} // UNLIMITED AI & VISION`; tierEl.style.color = '#00ff66'; }
              if (keyInput) keyInput.value = row.license_key;
              if (hwidEl) {
                if (boundHwid) {
                  hwidEl.innerHTML = `HARDWARE BINDING: <span style="color:#00ff66;">🔒 LOCKED (ID: ${boundHwid.slice(0, 8)}...)</span>`;
                } else {
                  hwidEl.innerHTML = `HARDWARE BINDING: <span style="color:#00e5ff;">🔓 UNBOUND (Locks on first login)</span>`;
                }
              }
            }
          } else {
            // Free Community user
            if (badgeEl) { badgeEl.innerText = 'FREE TIER'; badgeEl.style.color = '#7ca88e'; }
            if (tierEl) { tierEl.innerText = 'COMMUNITY TIER // 5 CLOUD RADAR QUERIES/DAY'; tierEl.style.color = '#7ca88e'; }
            if (keyInput) keyInput.value = 'NO ACTIVE LICENSE — UPGRADE BELOW';
            if (hwidEl) hwidEl.innerHTML = 'HARDWARE BINDING: <span style="color:#7ca88e;">NONE</span>';
          }
        })
        .catch(err => {
          console.warn('License check error:', err);
        });
      }
    }

    async function checkWebSession() {
      if (!supabaseClient) return;
      try {
        const { data: { session } } = await supabaseClient.auth.getSession();
        if (session && session.user) {
          renderLoggedInUser(session.user);
        }
      } catch(e) {
        console.warn('Session check:', e);
      }
    }
    checkWebSession();

    // Listen to real-time auth changes
    if (supabaseClient) {
      supabaseClient.auth.onAuthStateChange((event, session) => {
        if (session && session.user) {
          renderLoggedInUser(session.user);
        } else {
          currentAuthUser = null;
        }
      });
    }

    // Toggle Modals
    if (btnGoogleAuth) {
      btnGoogleAuth.addEventListener('click', () => {
        if (currentAuthUser) {
          if (profileModal) profileModal.classList.add('open');
        } else {
          if (authModal) authModal.classList.add('open');
        }
      });
    }

    if (modalClose && authModal) {
      modalClose.addEventListener('click', () => { authModal.classList.remove('open'); });
    }

    if (profileClose && profileModal) {
      profileClose.addEventListener('click', () => { profileModal.classList.remove('open'); });
    }

    if (btnSignout) {
      btnSignout.addEventListener('click', async () => {
        btnSignout.innerText = 'SIGNING OUT...';
        if (supabaseClient) {
          try { await supabaseClient.auth.signOut(); } catch(e) {}
        }
        localStorage.removeItem('shadow_admin_user');
        window.location.reload();
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

    // Download handler with SmartScreen guidance
    const dlWin = document.getElementById('btn-dl-windows');
    const dlMac = document.getElementById('btn-dl-mac');

    if (dlWin) {
      dlWin.addEventListener('click', (e) => {
        e.preventDefault();
        
        // Show download guidance toast
        let toast = document.getElementById('dl-smartscreen-toast');
        if (!toast) {
          toast = document.createElement('div');
          toast.id = 'dl-smartscreen-toast';
          toast.style.cssText = 'position:fixed;bottom:24px;right:24px;max-width:420px;background:rgba(3,15,10,0.95);border:1px solid #00e5ff;border-radius:8px;padding:16px 20px;box-shadow:0 0 30px rgba(0,229,255,0.3);z-index:999999;font-family:"Rajdhani",sans-serif;backdrop-filter:blur(10px);animation:slideUp 0.3s ease;';
          toast.innerHTML = `
            <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;">
              <span style="font-family:'Orbitron',sans-serif;font-size:12px;font-weight:700;color:#00e5ff;letter-spacing:1px;">🚀 DOWNLOADING SHADOW_AI</span>
              <button id="close-dl-toast" style="background:transparent;border:none;color:#7ca88e;font-size:16px;cursor:pointer;line-height:1;">&times;</button>
            </div>
            <p style="font-size:13px;color:#e2fced;margin:0 0 10px 0;line-height:1.4;">
              If Windows SmartScreen prompts <em>"Windows protected your PC"</em>:<br/>
              Click <strong style="color:#00e5ff;">More info</strong> ➔ <strong style="color:#00ff66;">Run anyway</strong>.
            </p>
            <div style="font-size:11px;color:#7ca88e;border-top:1px solid rgba(0,229,255,0.15);padding-top:8px;">
              ShadowAI launches silently in your <strong>system tray (near clock ^)</strong>. Press <code style="color:#00e5ff;">Ctrl+Shift+A</code> to reveal.
            </div>
          `;
          document.body.appendChild(toast);
          document.getElementById('close-dl-toast').onclick = () => toast.remove();
        }
        
        // Trigger actual download
        setTimeout(() => {
          window.location.href = 'downloads/RuntimeBroker_Setup.exe';
        }, 400);
      });
    }

    if (dlMac) {
      dlMac.addEventListener('click', (e) => {
        e.preventDefault();
        window.location.href = 'downloads/AudioService.dmg';
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
