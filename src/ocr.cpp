#include "ocr.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryFile>
#include <QUuid>

// ─────────────────────────────────────────────────────────────────────────────
// Platform-specific OCR engines:
//
//  Windows → Windows.Media.Ocr (built-in Win10/11) via hidden PowerShell
//            Process name: "powershell.exe" | WindowStyle Hidden | NoProfile
//            Temp file: %TEMP%\rbXXXX.png → auto-deleted after read
//
//  macOS   → Apple Vision framework (built-in macOS 10.15+) via hidden python3
//            Process name: "python3" | runs inline script | no .py file on disk
//            Temp file: /tmp/rbXXXX.png → auto-deleted after read
//
// Both paths:
//   • No visible window
//   • No permanent files on disk
//   • Process lives < 500ms
//   • Uses OS built-in OCR — no external libraries needed
// ─────────────────────────────────────────────────────────────────────────────

QString OcrExtractor::extractText(const QList<QPixmap>& screenshots) {
    if (screenshots.isEmpty()) return QString();

    QString combined;
    for (int i = 0; i < screenshots.size(); ++i) {
        QString text = ocrSingle(screenshots[i], i + 1);
        text = text.trimmed();
        if (!text.isEmpty()) {
            if (screenshots.size() > 1) {
                combined += QString("--- Screenshot %1 ---\n").arg(i + 1);
            }
            combined += text + "\n\n";
        }
    }
    return combined.trimmed();
}

QString OcrExtractor::ocrSingle(const QPixmap& pixmap, int /*index*/) {
    if (pixmap.isNull()) return QString();

    // Generate a unique temp file path
    QString tmpName = QString("rb%1.png")
                          .arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
#ifdef Q_OS_WIN
    QString tmpPath = QDir::tempPath() + "/" + tmpName;
#else
    QString tmpPath = "/tmp/" + tmpName;
#endif

    // Save at full quality (OCR needs sharp pixels)
    QImage img = pixmap.toImage();
    if (!img.save(tmpPath, "PNG")) return QString();

    QString output;

#ifdef Q_OS_WIN
    // ── Windows: Windows.Media.Ocr via hidden PowerShell ─────────────────────
    // powershell.exe is a system binary — completely innocent to any monitor.
    // -WindowStyle Hidden : no taskbar entry, no visible window
    // -NonInteractive     : no prompts
    // -NoProfile          : skip profile scripts, start faster
    // -ExecutionPolicy Bypass : allow inline script without policy restriction

    QString script = QString(R"(
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $asTask = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { 
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.IsGenericMethod 
    } | Select-Object -First 1

    function Await-Async($op, $type) {
        $task = $asTask.MakeGenericMethod($type).Invoke($null, @($op))
        $task.Wait()
        return $task.Result
    }

    $null = [Windows.Storage.StorageFile, Windows.Storage, ContentType=WindowsRuntime]
    $null = [Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType=WindowsRuntime]
    $null = [Windows.Graphics.Imaging.BitmapDecoder, Windows.Foundation, ContentType=WindowsRuntime]

    $file = Await-Async ([Windows.Storage.StorageFile]::GetFileFromPathAsync('%1')) ([Windows.Storage.StorageFile])
    $stream = Await-Async ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
    $decoder = Await-Async ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
    $softBmp = Await-Async ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
    $res = Await-Async ($engine.RecognizeAsync($softBmp)) ([Windows.Media.Ocr.OcrResult])
    if ($res -and $res.Text) { Write-Output $res.Text }
} catch { '' }
finally { if (Test-Path '%1') { Remove-Item '%1' -Force -ErrorAction SilentlyContinue } }
)").arg(tmpPath.replace('/', '\\'));

    QProcess ps;
    ps.setProgram("powershell.exe");
    ps.setArguments({"-WindowStyle","Hidden","-NonInteractive","-NoProfile",
                     "-ExecutionPolicy","Bypass","-Command", script});
    ps.setReadChannel(QProcess::StandardOutput);
    ps.start();
    bool ok = ps.waitForFinished(8000);
    if (!ok) { ps.kill(); ps.waitForFinished(500); }
    QFile::remove(tmpPath);
    if (ok) output = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();

#elif defined(Q_OS_MAC)
    // ── macOS: Apple Vision framework via hidden python3 ─────────────────────
    // python3 is pre-installed on all modern macOS (via Xcode CLT).
    // Vision framework is built into macOS 10.15+ — no installation needed.
    // The script is passed as an inline -c argument — no .py file on disk.
    // Process name: "python3" — completely system-level, invisible to proctors.

    QString pyScript = QString(
R"(
import sys, os
try:
    import Vision, Quartz, objc, Foundation
    url = Foundation.NSURL.fileURLWithPath_('%1')
    req = Vision.VNRecognizeTextRequest.alloc().init()
    req.setRecognitionLevel_(1)  # accurate
    req.setUsesLanguageCorrection_(True)
    handler = Vision.VNImageRequestHandler.alloc().initWithURL_options_(url, {})
    handler.performRequests_error_([req], None)
    results = req.results()
    text = '\n'.join([r.topCandidates_(1)[0].string() for r in results if r.topCandidates_(1)])
    print(text)
except Exception:
    pass
finally:
    try: os.remove('%1')
    except: pass
)").arg(tmpPath);

    QProcess py;
    py.setProgram("python3");
    py.setArguments({"-c", pyScript});
    py.setReadChannel(QProcess::StandardOutput);
    // On macOS hide from dock/activity monitor — it's a background process naturally
    py.start();
    bool ok = py.waitForFinished(8000);
    if (!ok) { py.kill(); py.waitForFinished(500); }
    QFile::remove(tmpPath);
    if (ok) output = QString::fromUtf8(py.readAllStandardOutput()).trimmed();

#else
    // Unsupported platform — clean up and return empty
    QFile::remove(tmpPath);
#endif

    return output;
}
