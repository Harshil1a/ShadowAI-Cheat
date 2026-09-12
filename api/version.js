module.exports = (req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Content-Type', 'application/json');
    res.status(200).json({
        latest_version: "2.4.1",
        min_version: "2.0.0",
        download_url: "https://shadow-ai-cheat.vercel.app/#downloads",
        release_notes: "Added Rewarded Solves Vault via sponsor tasks, persistent top profile & credits HUD in settings, and zero-key HWID synchronization."
    });
};
